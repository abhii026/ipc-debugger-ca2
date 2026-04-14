#ifndef LOGGER_H 
#define LOGGER_H 
 
/* ============================================================ 
 * logger.h  —  Module 2: Debugger & Logger 
 * Author     : Member 2 
 * Description: Declares the log_event function and log path 
 * ============================================================ */ 
 
#define LOG_FILE "logs/ipc_events.log" 
 
/* Log an IPC event to both terminal and log file 
 * module : "PIPE", "SHM", or "SIGNAL" 
 * event  : Short description of what happened 
 */ 
void log_event(const char *module, const char *event); 
 
/* Print all entries from the log file */ 
void view_logs(void); 
 
/* Delete all entries from the log file */ 
void clear_logs(void); 
 
#endif /* LOGGER_H */ 