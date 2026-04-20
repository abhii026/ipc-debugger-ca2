/*
 * IPC Debugger Backend — C Server  (fixed + enhanced)
 * Simulates Pipes, Message Queues, and Shared Memory
 * Exposes a simple HTTP API on port 8765 for the frontend.
 *
 * Compile:
 *   gcc -O2 -Wall -pthread -o ipc_debug_server ipc_debug_server.c
 * Run:
 *   ./ipc_debug_server
 *
 * Fixes applied
 * ─────────────
 * 1. buffer_used clamped after pipe_read subtraction (was missing).
 * 2. msg_count / bytes_transferred now updated on successful msgq_recv.
 * 3. reset() now clears g_lock_count (was left dirty).
 * 4. Action parser used raw strstr/atoi — replaced with token-safe helper
 *    to avoid "ch=" matching "bytes_transferred" etc.
 * 5. JSON buffer was a fixed 16 KB stack array — now heap-allocated to
 *    handle MAX_EVENTS=512 without overflow.
 * 6. sim_deadlock index guard: changed > to >= (off-by-one fix).
 * 7. add_process now zero-initialises the ProcessInfo struct (memset).
 * 8. Process pid field is set consistently before g_proc_count++ is used.
 * 9. CORS OPTIONS pre-flight now sends a proper Content-Length: 0 header.
 *10. throughput_bps field added to JSON channel output.
 *
 * Enhancements
 * ────────────
 * • GET /health — heartbeat endpoint for the frontend status indicator.
 * • Auto-simulation thread: generates synthetic IPC traffic when idle > 3 s,
 *   keeping the visualiser lively without manual interaction.
 * • Per-process cpu_time_us incremented each event for richer timeline data.
 * • Throughput computed as bytes_transferred / uptime_seconds per channel.
 * • resolve_deadlock action: clears deadlock state on all processes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define PORT            8765
#define MAX_EVENTS      512
#define SHM_SIZE        4096
#define MSG_SIZE        128
#define MAX_PROCESSES   8
#define MAX_CHANNELS    16
#define MAX_LOCKS       8

/* ─────────────────── Data Structures ─────────────────── */

typedef enum { IPC_PIPE, IPC_MSGQUEUE, IPC_SHMEM } IPCType;

typedef enum {
    EVT_WRITE, EVT_READ, EVT_LOCK, EVT_UNLOCK,
    EVT_DEADLOCK, EVT_BOTTLENECK, EVT_CREATE, EVT_DESTROY,
    EVT_BLOCK, EVT_UNBLOCK
} EventType;

typedef struct {
    long long timestamp_us;
    int       process_id;
    IPCType   ipc_type;
    EventType event;
    char      message[256];
    int       bytes;
    int       severity;   /* 0=info  1=warn  2=error */
    int       channel_id;
} IPCEvent;

typedef struct {
    char      name[32];
    int       pid;
    int       state;      /* 0=idle 1=running 2=blocked 3=deadlock */
    int       held_lock;
    int       waiting_lock;
    long long cpu_time_us;
    int       bytes_sent;
    int       bytes_recv;
    int       msg_count;
} ProcessInfo;

typedef struct {
    int     id;
    IPCType type;
    char    name[64];
    int     is_active;
    int     bytes_transferred;
    int     msg_count;
    int     reader_pid;
    int     writer_pid;
    int     is_blocked;
    int     buffer_used;
    int     buffer_capacity;
    double  throughput_bps;
} ChannelInfo;

typedef struct {
    int lock_id;
    int holder_pid;
    int waiters[MAX_PROCESSES];
    int waiter_count;
    int is_deadlock;
} LockState;

/* ─────────────────── Global State ─────────────────── */

static IPCEvent    g_events[MAX_EVENTS];
static int         g_event_count   = 0;
static ProcessInfo g_processes[MAX_PROCESSES];
static int         g_proc_count    = 0;
static ChannelInfo g_channels[MAX_CHANNELS];
static int         g_channel_count = 0;
static LockState   g_locks[MAX_LOCKS];
static int         g_lock_count    = 0;

static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static int    g_pipe_fds[2]  = {-1, -1};
static int    g_msgq_id      = -1;
static int    g_shm_id       = -1;
static void  *g_shm_ptr      = NULL;
static pthread_mutex_t g_shm_mutex = PTHREAD_MUTEX_INITIALIZER;

static long long g_start_us       = 0;
static long long g_last_action_us = 0;

/* ─────────────────── Helpers ─────────────────── */

static long long now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

/* FIX #4: token-safe parameter extractor */
static int param_int(const char *body, const char *key) {
    size_t klen = strlen(key);
    const char *p = body;
    while (p && *p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=')
            return atoi(p + klen + 1);
        p = strchr(p, '&');
        if (p) p++;
    }
    return 0;
}

static void param_str(const char *body, const char *key, char *out, int outlen) {
    size_t klen = strlen(key);
    const char *p = body;
    while (p && *p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *v = p + klen + 1;
            int i = 0;
            while (*v && *v != '&' && *v != '\r' && *v != '\n' && i < outlen - 1)
                out[i++] = *v++;
            out[i] = '\0';
            return;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    out[0] = '\0';
}

static void push_event(int pid, IPCType ipc, EventType evt,
                       const char *msg, int bytes, int sev, int ch_id) {
    pthread_mutex_lock(&g_state_mutex);
    if (g_event_count >= MAX_EVENTS) {
        memmove(g_events, g_events + 1, (MAX_EVENTS - 1) * sizeof(IPCEvent));
        g_event_count = MAX_EVENTS - 1;
    }
    IPCEvent *e     = &g_events[g_event_count++];
    e->timestamp_us = now_us();
    e->process_id   = pid;
    e->ipc_type     = ipc;
    e->event        = evt;
    strncpy(e->message, msg ? msg : "", 255);
    e->message[255] = '\0';
    e->bytes        = bytes;
    e->severity     = sev;
    e->channel_id   = ch_id;
    if (pid >= 0 && pid < g_proc_count)
        g_processes[pid].cpu_time_us += 500;
    pthread_mutex_unlock(&g_state_mutex);
}

/* ─────────────────── IPC Init ─────────────────── */

static int init_pipe(void) {
    if (g_pipe_fds[0] >= 0) return -1;
    if (pipe(g_pipe_fds) == -1) return -1;
    fcntl(g_pipe_fds[0], F_SETFL, O_NONBLOCK);
    fcntl(g_pipe_fds[1], F_SETFL, O_NONBLOCK);
    pthread_mutex_lock(&g_state_mutex);
    if (g_channel_count >= MAX_CHANNELS) { pthread_mutex_unlock(&g_state_mutex); return -1; }
    ChannelInfo *ch = &g_channels[g_channel_count];
    memset(ch, 0, sizeof(*ch));
    ch->id = g_channel_count; ch->type = IPC_PIPE; ch->is_active = 1;
    ch->buffer_capacity = 65536;
    snprintf(ch->name, 64, "Pipe #%d", g_channel_count);
    int id = g_channel_count++;
    pthread_mutex_unlock(&g_state_mutex);
    push_event(0, IPC_PIPE, EVT_CREATE, "Pipe created (fd pair)", 0, 0, id);
    return id;
}

static int init_msgqueue(void) {
    if (g_msgq_id >= 0) return -1;
    key_t key = ftok("/tmp", 'M' + g_channel_count);
    g_msgq_id = msgget(key, IPC_CREAT | 0666);
    if (g_msgq_id == -1) return -1;
    pthread_mutex_lock(&g_state_mutex);
    if (g_channel_count >= MAX_CHANNELS) { pthread_mutex_unlock(&g_state_mutex); return -1; }
    ChannelInfo *ch = &g_channels[g_channel_count];
    memset(ch, 0, sizeof(*ch));
    ch->id = g_channel_count; ch->type = IPC_MSGQUEUE; ch->is_active = 1;
    ch->buffer_capacity = 16384;
    snprintf(ch->name, 64, "MsgQ #%d (id=%d)", g_channel_count, g_msgq_id);
    int id = g_channel_count++;
    pthread_mutex_unlock(&g_state_mutex);
    push_event(0, IPC_MSGQUEUE, EVT_CREATE, "Message queue created", 0, 0, id);
    return id;
}

static int init_shmem(void) {
    if (g_shm_id >= 0) return -1;
    key_t key = ftok("/tmp", 'S' + g_channel_count);
    g_shm_id  = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (g_shm_id == -1) return -1;
    g_shm_ptr = shmat(g_shm_id, NULL, 0);
    if (g_shm_ptr == (void *)-1) { g_shm_ptr = NULL; return -1; }
    memset(g_shm_ptr, 0, SHM_SIZE);
    pthread_mutex_lock(&g_state_mutex);
    if (g_channel_count >= MAX_CHANNELS) { pthread_mutex_unlock(&g_state_mutex); return -1; }
    ChannelInfo *ch = &g_channels[g_channel_count];
    memset(ch, 0, sizeof(*ch));
    ch->id = g_channel_count; ch->type = IPC_SHMEM; ch->is_active = 1;
    ch->buffer_capacity = SHM_SIZE;
    snprintf(ch->name, 64, "SHM #%d (id=%d)", g_channel_count, g_shm_id);
    int id = g_channel_count++;
    pthread_mutex_unlock(&g_state_mutex);
    push_event(0, IPC_SHMEM, EVT_CREATE, "Shared memory segment created", 0, 0, id);
    return id;
}

/* ─────────────────── Simulation Actions ─────────────── */

struct msgbuf { long mtype; char mtext[MSG_SIZE]; };

static void sim_pipe_write(int pid, int ch_id, const char *data, int len) {
    if (g_pipe_fds[1] < 0) return;
    char msg[256];
    int n = (int)write(g_pipe_fds[1], data, (size_t)len);
    pthread_mutex_lock(&g_state_mutex);
    if (ch_id < 0 || ch_id >= g_channel_count) { pthread_mutex_unlock(&g_state_mutex); return; }
    if (n < 0) {
        g_channels[ch_id].is_blocked = 1;
        pthread_mutex_unlock(&g_state_mutex);
        snprintf(msg, 256, "P%d: pipe write FAILED (EAGAIN — buffer full)", pid);
        push_event(pid, IPC_PIPE, EVT_BOTTLENECK, msg, 0, 2, ch_id);
    } else {
        g_channels[ch_id].bytes_transferred += n;
        g_channels[ch_id].buffer_used       += n;
        int used = g_channels[ch_id].buffer_used;
        int cap  = g_channels[ch_id].buffer_capacity;
        if (pid >= 0 && pid < g_proc_count) g_processes[pid].bytes_sent += n;
        pthread_mutex_unlock(&g_state_mutex);
        snprintf(msg, 256, "P%d: wrote %d bytes to pipe", pid, n);
        push_event(pid, IPC_PIPE, EVT_WRITE, msg, n, 0, ch_id);
        if (used > cap * 8 / 10)
            push_event(pid, IPC_PIPE, EVT_BOTTLENECK, "Pipe buffer >80% full", n, 1, ch_id);
    }
}

static void sim_pipe_read(int pid, int ch_id) {
    if (g_pipe_fds[0] < 0) return;
    char buf[512], msg[256];
    int n = (int)read(g_pipe_fds[0], buf, sizeof(buf));
    pthread_mutex_lock(&g_state_mutex);
    if (ch_id < 0 || ch_id >= g_channel_count) { pthread_mutex_unlock(&g_state_mutex); return; }
    if (n <= 0) {
        g_channels[ch_id].is_blocked = 0;
        pthread_mutex_unlock(&g_state_mutex);
        snprintf(msg, 256, "P%d: pipe read — no data available", pid);
        push_event(pid, IPC_PIPE, EVT_BLOCK, msg, 0, 1, ch_id);
    } else {
        /* FIX #1: clamp buffer_used */
        g_channels[ch_id].buffer_used -= n;
        if (g_channels[ch_id].buffer_used < 0) g_channels[ch_id].buffer_used = 0;
        g_channels[ch_id].is_blocked   = 0;
        if (pid >= 0 && pid < g_proc_count) g_processes[pid].bytes_recv += n;
        pthread_mutex_unlock(&g_state_mutex);
        snprintf(msg, 256, "P%d: read %d bytes from pipe", pid, n);
        push_event(pid, IPC_PIPE, EVT_READ, msg, n, 0, ch_id);
    }
}

static void sim_msgq_send(int pid, int ch_id, int mtype, const char *text) {
    if (g_msgq_id < 0) return;
    struct msgbuf mb; char msg[256];
    mb.mtype = mtype > 0 ? mtype : 1;
    strncpy(mb.mtext, text, MSG_SIZE - 1); mb.mtext[MSG_SIZE-1]='\0';
    int r = (int)msgsnd(g_msgq_id, &mb, MSG_SIZE, IPC_NOWAIT);
    pthread_mutex_lock(&g_state_mutex);
    if (ch_id < 0 || ch_id >= g_channel_count) { pthread_mutex_unlock(&g_state_mutex); return; }
    if (r < 0) {
        pthread_mutex_unlock(&g_state_mutex);
        snprintf(msg, 256, "P%d: msgsnd FAILED — queue full", pid);
        push_event(pid, IPC_MSGQUEUE, EVT_BOTTLENECK, msg, 0, 2, ch_id);
    } else {
        g_channels[ch_id].msg_count++;
        g_channels[ch_id].bytes_transferred += MSG_SIZE;
        if (pid >= 0 && pid < g_proc_count) {
            g_processes[pid].bytes_sent += MSG_SIZE;
            g_processes[pid].msg_count++;
        }
        pthread_mutex_unlock(&g_state_mutex);
        snprintf(msg, 256, "P%d: sent msg type=%ld '%.60s'", pid, mb.mtype, mb.mtext);
        push_event(pid, IPC_MSGQUEUE, EVT_WRITE, msg, MSG_SIZE, 0, ch_id);
    }
}

static void sim_msgq_recv(int pid, int ch_id, int mtype) {
    if (g_msgq_id < 0) return;
    struct msgbuf mb; char msg[256];
    int r = (int)msgrcv(g_msgq_id, &mb, MSG_SIZE, mtype, IPC_NOWAIT);
    pthread_mutex_lock(&g_state_mutex);
    if (ch_id < 0 || ch_id >= g_channel_count) { pthread_mutex_unlock(&g_state_mutex); return; }
    if (r < 0) {
        pthread_mutex_unlock(&g_state_mutex);
        snprintf(msg, 256, "P%d: msgrcv FAILED — no message (EAGAIN)", pid);
        push_event(pid, IPC_MSGQUEUE, EVT_BLOCK, msg, 0, 1, ch_id);
    } else {
        /* FIX #2: update msg_count and bytes on recv */
        if (g_channels[ch_id].msg_count > 0) g_channels[ch_id].msg_count--;
        g_channels[ch_id].bytes_transferred += r;
        if (pid >= 0 && pid < g_proc_count)
            g_processes[pid].bytes_recv += r;
        pthread_mutex_unlock(&g_state_mutex);
        snprintf(msg, 256, "P%d: received msg type=%ld '%.60s'", pid, mb.mtype, mb.mtext);
        push_event(pid, IPC_MSGQUEUE, EVT_READ, msg, r, 0, ch_id);
    }
}

static void sim_shm_write(int pid, int ch_id, int offset, const char *data, int len) {
    if (!g_shm_ptr) return;
    char msg[256];
    if (offset < 0) offset = 0;
    if (offset + len > SHM_SIZE) len = SHM_SIZE - offset;
    if (len <= 0) return;
    push_event(pid, IPC_SHMEM, EVT_LOCK, "Acquiring SHM mutex for write", 0, 0, ch_id);
    pthread_mutex_lock(&g_shm_mutex);
    memcpy((char *)g_shm_ptr + offset, data, (size_t)len);
    pthread_mutex_lock(&g_state_mutex);
    if (ch_id >= 0 && ch_id < g_channel_count) {
        g_channels[ch_id].bytes_transferred += len;
        g_channels[ch_id].buffer_used        = offset + len;
        if (pid >= 0 && pid < g_proc_count) g_processes[pid].bytes_sent += len;
    }
    pthread_mutex_unlock(&g_state_mutex);
    pthread_mutex_unlock(&g_shm_mutex);
    snprintf(msg, 256, "P%d: wrote %d bytes to SHM offset=%d", pid, len, offset);
    push_event(pid, IPC_SHMEM, EVT_WRITE, msg, len, 0, ch_id);
    push_event(pid, IPC_SHMEM, EVT_UNLOCK, "Released SHM mutex", 0, 0, ch_id);
}

static void sim_shm_read(int pid, int ch_id, int offset, int len) {
    if (!g_shm_ptr) return;
    char msg[256];
    if (offset < 0) offset = 0;
    if (offset + len > SHM_SIZE) len = SHM_SIZE - offset;
    if (len <= 0) return;
    push_event(pid, IPC_SHMEM, EVT_LOCK, "Acquiring SHM mutex for read", 0, 0, ch_id);
    pthread_mutex_lock(&g_shm_mutex);
    if (pid >= 0 && pid < g_proc_count) {
        pthread_mutex_lock(&g_state_mutex);
        g_processes[pid].bytes_recv += len;
        pthread_mutex_unlock(&g_state_mutex);
    }
    pthread_mutex_unlock(&g_shm_mutex);
    snprintf(msg, 256, "P%d: read %d bytes from SHM offset=%d", pid, len, offset);
    push_event(pid, IPC_SHMEM, EVT_READ, msg, len, 0, ch_id);
    push_event(pid, IPC_SHMEM, EVT_UNLOCK, "Released SHM mutex", 0, 0, ch_id);
}

/* FIX #6: use >= for index check */
static void sim_deadlock(int pid1, int pid2) {
    char msg[256];
    snprintf(msg, 256, "DEADLOCK DETECTED: P%d <-> P%d circular wait", pid1, pid2);
    push_event(pid1, IPC_SHMEM, EVT_DEADLOCK, msg, 0, 2, -1);
    snprintf(msg, 256, "P%d holds Lock-A, waiting Lock-B", pid1);
    push_event(pid1, IPC_SHMEM, EVT_BLOCK, msg, 0, 2, -1);
    snprintf(msg, 256, "P%d holds Lock-B, waiting Lock-A", pid2);
    push_event(pid2, IPC_SHMEM, EVT_BLOCK, msg, 0, 2, -1);
    pthread_mutex_lock(&g_state_mutex);
    if (pid1 >= 0 && pid1 < g_proc_count) g_processes[pid1].state = 3;
    if (pid2 >= 0 && pid2 < g_proc_count) g_processes[pid2].state = 3;
    pthread_mutex_unlock(&g_state_mutex);
}

/* ─────────────────── Auto-Simulation Thread ───────────── */

static void *auto_sim_thread(void *arg) {
    (void)arg;
    sleep(2);
    for (;;) {
        usleep(800000);
        if ((now_us() - g_last_action_us) < 3000000) continue;

        pthread_mutex_lock(&g_state_mutex);
        int procs   = g_proc_count;
        int pipe_ok = g_pipe_fds[1] >= 0;
        int mq_ok   = g_msgq_id    >= 0;
        int shm_ok  = g_shm_ptr    != NULL;
        int pipe_ch = -1, mq_ch = -1, shm_ch = -1;
        for (int i = 0; i < g_channel_count; i++) {
            if (g_channels[i].type == IPC_PIPE     && pipe_ch < 0) pipe_ch = i;
            if (g_channels[i].type == IPC_MSGQUEUE && mq_ch   < 0) mq_ch   = i;
            if (g_channels[i].type == IPC_SHMEM    && shm_ch  < 0) shm_ch  = i;
        }
        pthread_mutex_unlock(&g_state_mutex);

        if (procs < 2 || (!pipe_ok && !mq_ok && !shm_ok)) continue;

        static int tog = 0; tog++;
        int p0 = 0, p1 = 1 % procs;
        if (pipe_ok && pipe_ch >= 0 && (tog % 3) == 0) {
            char d[64]; snprintf(d, 64, "auto-ping-%d", tog);
            sim_pipe_write(p0, pipe_ch, d, (int)strlen(d));
            sim_pipe_read(p1, pipe_ch);
        }
        if (mq_ok && mq_ch >= 0 && (tog % 3) == 1) {
            char d[64]; snprintf(d, 64, "mq-auto-%d", tog);
            sim_msgq_send(p0, mq_ch, 1, d);
            sim_msgq_recv(p1, mq_ch, 0);
        }
        if (shm_ok && shm_ch >= 0 && (tog % 3) == 2) {
            char d[64]; snprintf(d, 64, "shm-auto-%d", tog);
            int off = (tog * 32) % (SHM_SIZE - 64);
            sim_shm_write(p0, shm_ch, off, d, (int)strlen(d));
            sim_shm_read(p1, shm_ch, off, (int)strlen(d));
        }
    }
    return NULL;
}

/* ─────────────────── HTTP Helpers ─────────────────── */

static const char *ipc_type_str(IPCType t) {
    switch (t) {
        case IPC_PIPE:     return "pipe";
        case IPC_MSGQUEUE: return "msgqueue";
        case IPC_SHMEM:    return "shmem";
        default:           return "unknown";
    }
}

static const char *evt_str(EventType e) {
    switch (e) {
        case EVT_WRITE:      return "write";
        case EVT_READ:       return "read";
        case EVT_LOCK:       return "lock";
        case EVT_UNLOCK:     return "unlock";
        case EVT_DEADLOCK:   return "deadlock";
        case EVT_BOTTLENECK: return "bottleneck";
        case EVT_CREATE:     return "create";
        case EVT_DESTROY:    return "destroy";
        case EVT_BLOCK:      return "block";
        case EVT_UNBLOCK:    return "unblock";
        default:             return "unknown";
    }
}

/* FIX #9: unified header writer */
static void write_headers(int fd, int content_len) {
    char hdr[512];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        content_len);
    write(fd, hdr, n);
}

/* FIX #5: heap-allocated JSON */
static void handle_state(int fd) {
    long long uptime = (now_us() - g_start_us) / 1000000 + 1;
    size_t cap = 4096
               + (size_t)g_event_count   * 300
               + (size_t)g_proc_count    * 150
               + (size_t)g_channel_count * 200;
    char *json = (char *)malloc(cap);
    if (!json) { write_headers(fd, 2); write(fd, "{}", 2); return; }

    size_t pos = 0;
    pthread_mutex_lock(&g_state_mutex);

    pos += (size_t)snprintf(json+pos, cap-pos, "{\"events\":[");
    for (int i = 0; i < g_event_count && pos + 400 < cap; i++) {
        IPCEvent *e = &g_events[i];
        /* escape quotes in message */
        char esc[512]; int ei = 0;
        for (int j = 0; e->message[j] && ei < 508; j++) {
            if (e->message[j]=='"' || e->message[j]=='\\') esc[ei++]='\\';
            esc[ei++] = e->message[j];
        }
        esc[ei] = '\0';
        pos += (size_t)snprintf(json+pos, cap-pos,
            "%s{\"ts\":%lld,\"pid\":%d,\"ipc\":\"%s\","
            "\"evt\":\"%s\",\"msg\":\"%s\",\"bytes\":%d,"
            "\"sev\":%d,\"ch\":%d}",
            i?",":"", e->timestamp_us, e->process_id,
            ipc_type_str(e->ipc_type), evt_str(e->event),
            esc, e->bytes, e->severity, e->channel_id);
    }
    pos += (size_t)snprintf(json+pos, cap-pos, "],\"processes\":[");
    for (int i = 0; i < g_proc_count && pos + 200 < cap; i++) {
        ProcessInfo *p = &g_processes[i];
        pos += (size_t)snprintf(json+pos, cap-pos,
            "%s{\"id\":%d,\"name\":\"%s\",\"state\":%d,"
            "\"bytes_sent\":%d,\"bytes_recv\":%d,"
            "\"msg_count\":%d,\"cpu_us\":%lld}",
            i?",":"", p->pid, p->name, p->state,
            p->bytes_sent, p->bytes_recv, p->msg_count, p->cpu_time_us);
    }
    pos += (size_t)snprintf(json+pos, cap-pos, "],\"channels\":[");
    for (int i = 0; i < g_channel_count && pos + 250 < cap; i++) {
        ChannelInfo *c = &g_channels[i];
        c->throughput_bps = (double)c->bytes_transferred / (double)uptime;
        pos += (size_t)snprintf(json+pos, cap-pos,
            "%s{\"id\":%d,\"type\":\"%s\",\"name\":\"%s\","
            "\"active\":%d,\"bytes\":%d,\"msgs\":%d,"
            "\"blocked\":%d,\"buf_used\":%d,\"buf_cap\":%d,"
            "\"throughput\":%.1f}",
            i?",":"", c->id, ipc_type_str(c->type), c->name,
            c->is_active, c->bytes_transferred, c->msg_count,
            c->is_blocked, c->buffer_used, c->buffer_capacity,
            c->throughput_bps);
    }
    pos += (size_t)snprintf(json+pos, cap-pos, "],\"uptime\":%lld}", uptime);
    pthread_mutex_unlock(&g_state_mutex);

    write_headers(fd, (int)pos);
    write(fd, json, pos);
    free(json);
}

static void handle_action(int fd, const char *body) {
    char action[32]={}, data[MSG_SIZE]={};
    int pid    = param_int(body, "pid");
    int ch     = param_int(body, "ch");
    int offset = param_int(body, "offset");
    int mtype  = param_int(body, "mtype"); if (!mtype) mtype=1;
    int len    = param_int(body, "len");   if (!len)   len=16;
    param_str(body, "action", action, sizeof(action));
    param_str(body, "data",   data,   sizeof(data));
    if (!data[0]) snprintf(data, MSG_SIZE, "Hello from P%d @ t=%lld", pid, now_us());

    g_last_action_us = now_us();

    if      (!strcmp(action,"init_pipe"))   init_pipe();
    else if (!strcmp(action,"init_mq"))     init_msgqueue();
    else if (!strcmp(action,"init_shm"))    init_shmem();
    else if (!strcmp(action,"pipe_write"))  sim_pipe_write(pid,ch,data,(int)strlen(data));
    else if (!strcmp(action,"pipe_read"))   sim_pipe_read(pid,ch);
    else if (!strcmp(action,"mq_send"))     sim_msgq_send(pid,ch,mtype,data);
    else if (!strcmp(action,"mq_recv"))     sim_msgq_recv(pid,ch,mtype);
    else if (!strcmp(action,"shm_write"))   sim_shm_write(pid,ch,offset,data,(int)strlen(data));
    else if (!strcmp(action,"shm_read"))    sim_shm_read(pid,ch,offset,len);
    else if (!strcmp(action,"deadlock"))    sim_deadlock(pid,ch);
    else if (!strcmp(action,"resolve_deadlock")) {
        pthread_mutex_lock(&g_state_mutex);
        for (int i=0;i<g_proc_count;i++)
            if (g_processes[i].state==3) g_processes[i].state=0;
        pthread_mutex_unlock(&g_state_mutex);
        push_event(pid,IPC_SHMEM,EVT_UNBLOCK,"Deadlock resolved — processes unblocked",0,0,-1);
    }
    else if (!strcmp(action,"add_process")) {
        pthread_mutex_lock(&g_state_mutex);
        if (g_proc_count < MAX_PROCESSES) {
            /* FIX #7: zero-initialise */
            ProcessInfo *pr = &g_processes[g_proc_count];
            memset(pr, 0, sizeof(*pr));
            pr->pid   = g_proc_count;
            snprintf(pr->name, 32, "Process-%d", g_proc_count);
            pr->state = 1;
            int newpid = g_proc_count++;
            char msg[64]; snprintf(msg,64,"Process-%d spawned",newpid);
            pthread_mutex_unlock(&g_state_mutex);
            push_event(newpid,IPC_PIPE,EVT_CREATE,msg,0,0,-1);
        } else pthread_mutex_unlock(&g_state_mutex);
    }
    else if (!strcmp(action,"reset")) {
        pthread_mutex_lock(&g_state_mutex);
        g_event_count=0; g_proc_count=0; g_channel_count=0;
        /* FIX #3: also reset lock_count */
        g_lock_count=0;
        if (g_pipe_fds[0]>=0){ close(g_pipe_fds[0]); close(g_pipe_fds[1]);
                                g_pipe_fds[0]=g_pipe_fds[1]=-1; }
        if (g_msgq_id>=0){  msgctl(g_msgq_id,IPC_RMID,NULL); g_msgq_id=-1; }
        if (g_shm_ptr)  {  shmdt(g_shm_ptr); g_shm_ptr=NULL; }
        if (g_shm_id>=0){  shmctl(g_shm_id,IPC_RMID,NULL);  g_shm_id=-1; }
        pthread_mutex_unlock(&g_state_mutex);
    }

    const char *r="{\"ok\":true}";
    write_headers(fd,(int)strlen(r));
    write(fd,r,strlen(r));
}

static void handle_health(int fd) {
    const char *r="{\"status\":\"ok\"}";
    write_headers(fd,(int)strlen(r));
    write(fd,r,strlen(r));
}

static void *handle_client(void *arg) {
    int fd = *(int *)arg; free(arg);
    char buf[4096]={};
    int n=(int)read(fd,buf,sizeof(buf)-1);
    if (n<=0){ close(fd); return NULL; }
    buf[n]='\0';
    if (!strncmp(buf,"OPTIONS",7)){ write_headers(fd,0); close(fd); return NULL; }
    if      (strstr(buf,"GET /health"))      handle_health(fd);
    else if (strstr(buf,"GET /state"))       handle_state(fd);
    else if (strstr(buf,"POST /action")) {
        char *body=strstr(buf,"\r\n\r\n");
        handle_action(fd,body?body+4:"");
    } else {
        const char *nr="HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(fd,nr,strlen(nr));
    }
    close(fd); return NULL;
}

int main(void) {
    signal(SIGPIPE,SIG_IGN);
    g_start_us = g_last_action_us = now_us();

    int srv=socket(AF_INET,SOCK_STREAM,0);
    if (srv<0){ perror("socket"); return 1; }
    int opt=1; setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(PORT);
    addr.sin_addr.s_addr=INADDR_ANY;
    if (bind(srv,(struct sockaddr*)&addr,sizeof(addr))<0){ perror("bind"); return 1; }
    listen(srv,16);
    fprintf(stderr,"[IPC-Debugger] Listening on :%d\n",PORT);
    fprintf(stderr,"[IPC-Debugger] Endpoints: GET /health  GET /state  POST /action\n");

    pthread_t sim_t;
    pthread_create(&sim_t,NULL,auto_sim_thread,NULL);
    pthread_detach(sim_t);

    for(;;){
        int *cfd=malloc(sizeof(int));
        if (!cfd) continue;
        *cfd=accept(srv,NULL,NULL);
        if (*cfd<0){ free(cfd); continue; }
        pthread_t t;
        pthread_create(&t,NULL,handle_client,cfd);
        pthread_detach(t);
    }
    return 0;
}
