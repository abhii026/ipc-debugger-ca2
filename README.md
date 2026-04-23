# IPC Debugger – Inter-Process Communication Analyzer

## Overview

IPC Debugger is a system-level project designed to simulate and analyze Inter-Process Communication (IPC) mechanisms. It provides a real-time environment to observe how processes communicate using Pipes, Message Queues, and Shared Memory.

The project combines a C-based backend server with a web-based frontend dashboard. The backend handles IPC operations and exposes APIs, while the frontend visualizes processes, communication channels, and system events.

This project helps in understanding core operating system concepts through practical implementation and interactive visualization.

---

## Features

- Simulation of multiple processes  
- Support for IPC mechanisms:
  - Pipes  
  - Message Queues  
  - Shared Memory  
- Real-time communication between processes  
- Event logging system with timestamps  
- Deadlock simulation and detection  
- Bottleneck detection (buffer overflow conditions)  
- Process state tracking (running, blocked, deadlock)  
- Interactive frontend dashboard  
- REST-like API using C socket programming  

---

## Project Structure


---

## Technologies Used

### Programming Languages
- C  
- HTML  
- CSS  
- JavaScript  

### Libraries and Tools
- POSIX IPC (pipe, message queues, shared memory)  
- pthread (multithreading)  
- Socket programming (TCP/IP)  
- Standard C libraries  

### Other Tools
- GitHub (version control)  
- Web browser (for frontend interface)  

---

## How to Run the Project
```
ipc-debugger/
│
├── logs/
│ └── ipc_events.log
│
├── main-module/
│ ├── ipc_debug_server.c # Backend C server
│ └── ipc_debugger.html # Frontend dashboard
│
├── report/ # Project report files
│
├── main.c # Entry point (CLI)
├── Makefile # Build automation
├── ipc_debug_server # Compiled binary
├── README.md # Documentation
└── .gitignore
```
### Step 1: Compile the Backend Server

```bash
gcc ipc_debug_server.c -o ipc_debug_server -lpthread

Step 2: Run the Server
./ipc_debug_server

The server will start on:
http://localhost:8765

Step 3: Open the Frontend

Open the file ipc_debugger.html in your browser.

Step 4: Use the Dashboard
Spawn processes
Initialize IPC channels (Pipe, MQ, SHM)
Perform read/write operations
Simulate deadlock
Monitor logs and system behavior
API Endpoints
GET /health
Checks server status
GET /state
Returns current system state (events, processes, channels)
POST /action
Performs actions such as:
init_pipe
init_mq
init_shm
pipe_write / pipe_read
mq_send / mq_recv
shm_write / shm_read
deadlock
resolve_deadlock
add_process
reset
Key Concepts Covered
Inter-Process Communication (IPC)
Process synchronization
Deadlock detection
Buffer management and bottlenecks
Socket programming
Multithreading in C
GitHub Repository

Repository Name: IPC Debugger

GitHub Link: abhii026/ipc-debugger-ca2: CA2 Project — IPC Debugger in C | BTech CSE

Project Demonstration

YouTube Video Link:
https://youtu.be/wBktMEqd0wA?si=ZGyK3mbHs-pREwUp

Conclusion

This project provides a practical implementation of IPC mechanisms with real-time visualization. It helps in understanding how processes communicate and how system-level issues such as deadlocks and bottlenecks occur.

Future Scope
Add graphical analytics and charts
Support additional IPC mechanisms (semaphores, sockets)
Improve UI using modern frameworks
Add automated debugging insights
Authors
Aditya Kumar Singh
Abhishek Singh
Venkateswar Reddy Boggeti
