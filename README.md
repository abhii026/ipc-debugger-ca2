# 🔌 IPC Debugger
### Inter-Process Communication Analyzer — Simulate, Visualize, Debug

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css3&logoColor=white)
![JavaScript](https://img.shields.io/badge/JavaScript-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![GitHub](https://img.shields.io/badge/GitHub-181717?style=for-the-badge&logo=github&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)

---

## 📖 Overview

**IPC Debugger** is a system-level project that simulates and analyzes Inter-Process Communication (IPC) mechanisms in real time. It combines a **C-based backend server** with an interactive **web frontend dashboard** to help visualize how processes communicate using Pipes, Message Queues, and Shared Memory.

Built as a practical OS learning tool, it covers deadlock simulation, bottleneck detection, and process state tracking — all through an interactive browser interface.

---

## ✨ Features

| Feature | Description |
|---|---|
| 🔁 Multi-Process Simulation | Spawn and manage multiple concurrent processes |
| 📡 IPC Mechanisms | Pipes, Message Queues, and Shared Memory |
| 📋 Event Logging | Timestamped logs for all IPC events |
| 🔒 Deadlock Simulation | Trigger and resolve deadlock scenarios |
| ⚠️ Bottleneck Detection | Buffer overflow condition analysis |
| 📊 Process State Tracking | Running, blocked, and deadlock states |
| 🌐 REST-like API | C socket-based HTTP endpoints |
| 🖥️ Live Dashboard | Interactive browser-based frontend |

---

## 🛠️ Technologies Used

### Languages

![C](https://img.shields.io/badge/C-00599C?style=flat-square&logo=c&logoColor=white)
![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=flat-square&logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=flat-square&logo=css3&logoColor=white)
![JavaScript](https://img.shields.io/badge/JavaScript-F7DF1E?style=flat-square&logo=javascript&logoColor=black)

### Libraries & APIs

![POSIX](https://img.shields.io/badge/POSIX_IPC-4CAF50?style=flat-square&logo=linux&logoColor=white)
![pthreads](https://img.shields.io/badge/pthreads-9C27B0?style=flat-square&logo=linux&logoColor=white)
![TCP/IP](https://img.shields.io/badge/TCP%2FIP_Sockets-607D8B?style=flat-square&logo=cloudflare&logoColor=white)

### Tools

![GCC](https://img.shields.io/badge/GCC-00599C?style=flat-square&logo=gnu&logoColor=white)
![Make](https://img.shields.io/badge/Makefile-EF5350?style=flat-square&logo=cmake&logoColor=white)
![GitHub](https://img.shields.io/badge/GitHub-181717?style=flat-square&logo=github&logoColor=white)

---

## 📁 Project Structure

```
ipc-debugger/
│
├── logs/
│   └── ipc_events.log          # Runtime event log
│
├── main-module/
│   ├── ipc_debug_server.c      # Backend C server
│   └── ipc_debugger.html       # Frontend dashboard
│
├── report/                     # Project report files
│
├── main.c                      # Entry point (CLI)
├── Makefile                    # Build automation
├── ipc_debug_server            # Compiled binary
├── README.md                   # Documentation
└── .gitignore
```

---

## 🚀 Getting Started

### Step 1 — Compile the backend server

```bash
gcc ipc_debug_server.c -o ipc_debug_server -lpthread
```

### Step 2 — Run the server

```bash
./ipc_debug_server
```

> The server starts on **http://localhost:8765**

### Step 3 — Open the frontend

Open `main-module/ipc_debugger.html` in your browser (double-click or use `open`).

### Step 4 — Use the dashboard

- Spawn processes
- Initialize IPC channels (Pipe, MQ, SHM)
- Perform read/write operations
- Simulate and resolve deadlocks
- Monitor logs and system behavior in real time

---

## 🔌 API Reference

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/health` | Server health check |
| `GET` | `/state` | Returns full system state (events, processes, channels) |
| `POST` | `/action` | Performs an IPC action (see actions below) |

### POST `/action` — Supported Actions

```
init_pipe       init_mq         init_shm
pipe_write      pipe_read
mq_send         mq_recv
shm_write       shm_read
deadlock        resolve_deadlock
add_process     reset
```

---

## 💡 Key Concepts Covered

- Inter-Process Communication (IPC)
- Process synchronization
- Deadlock detection and resolution
- Buffer management and bottleneck analysis
- Socket programming in C
- Multithreading with pthreads
- POSIX APIs and OS internals

---

## 🔭 Future Scope

- 📈 Graphical analytics and live charts
- 🔐 Support for additional IPC mechanisms (semaphores, sockets)
- 🎨 Modern framework UI (React / Vue)
- 🤖 Automated debugging insights and suggestions

---

## 🔗 Links

- 🔗 **GitHub Repository:** [abhii026/ipc-debugger-ca2](https://github.com/abhii026/ipc-debugger-ca2)
- ▶️ **Demo Video:** [Watch on YouTube](https://youtu.be/wBktMEqd0wA?si=ZGyK3mbHs-pREwUp)

---

## 👥 Authors

| Name | Role |
|---|---|
| **Aditya Kumar Singh** | BTech CSE |
| **Abhishek Singh** | BTech CSE |
| **Venkateswar Reddy Boggeti** | BTech CSE |

---

## 📄 License

This project is open source and available under the [MIT License](LICENSE).

---
