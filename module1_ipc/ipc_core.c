#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include "ipc_core.h"
#include "../module2_logger/logger.h"

/* ================= PIPE DEMO ================= */
void run_pipe_demo(void) {
    int fd[2];
    pid_t pid;
    char write_msg[] = "Hello from Parent via Pipe!";
    char read_buf[100];

    log_event("PIPE", "Demo started");

    if (pipe(fd) == -1) {
        perror("Pipe failed");
        return;
    }

    pid = fork();

    if (pid == 0) {
        // CHILD
        close(fd[1]);
        read(fd[0], read_buf, sizeof(read_buf));
        printf("[CHILD] Received: %s\n", read_buf);
        log_event("PIPE", "Child received message");
        close(fd[0]);
        exit(0);
    } else {
        // PARENT
        close(fd[0]);
        write(fd[1], write_msg, strlen(write_msg) + 1);
        printf("[PARENT] Sent: %s\n", write_msg);
        log_event("PIPE", "Parent sent message");
        close(fd[1]);
        wait(NULL);
    }

    log_event("PIPE", "Demo completed");
}

/* ================= SHARED MEMORY ================= */
#define SHM_KEY 1234
#define SHM_SIZE 100

void run_shared_memory_demo(void) {
    int shmid;
    char *ptr;
    pid_t pid;

    log_event("SHM", "Demo started");

    shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);

    if (shmid < 0) {
        perror("shmget failed");
        return;
    }

    pid = fork();

    if (pid == 0) {
        sleep(1);
        ptr = (char *)shmat(shmid, NULL, 0);
        printf("[CHILD] Read: %s\n", ptr);
        log_event("SHM", "Child read data");
        shmdt(ptr);
        exit(0);
    } else {
        ptr = (char *)shmat(shmid, NULL, 0);
        sprintf(ptr, "Hello from Parent (Shared Memory)");
        printf("[PARENT] Written: %s\n", ptr);
        log_event("SHM", "Parent wrote data");
        shmdt(ptr);
        wait(NULL);
        shmctl(shmid, IPC_RMID, NULL);
    }

    log_event("SHM", "Demo completed");
}

/* ================= SIGNAL DEMO ================= */

void handler(int sig) {
    printf("[CHILD] Signal received: %d\n", sig);
}

void run_signal_demo(void) {
    pid_t pid;

    log_event("SIGNAL", "Demo started");

    pid = fork();

    if (pid == 0) {
        signal(SIGUSR1, handler);
        printf("[CHILD] Waiting for signal...\n");
        pause();
        exit(0);
    } else {
        sleep(1);
        printf("[PARENT] Sending signal...\n");
        kill(pid, SIGUSR1);
        wait(NULL);
    }

    log_event("SIGNAL", "Demo completed");
}