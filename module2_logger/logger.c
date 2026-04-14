#include <stdio.h> 
#include <stdlib.h> 
#include <time.h> 
#include <string.h> 
#include "logger.h" 
 
/* ============================================================ 
 * logger.c  —  Module 2: Debugger & Logger 
 * ============================================================ */ 
 
/* ───────────────────────────────────────────── 
 * ANSI color codes for terminal output 
 * ───────────────────────────────────────────── */ 
#define ANSI_GREEN  "[0;32m" 
#define ANSI_CYAN   "[0;36m" 
#define ANSI_YELLOW "[0;33m" 
#define ANSI_RESET  "[0m" 
 
/* Returns current timestamp as a formatted string */ 
static void get_timestamp(char *buf, size_t size) { 
    time_t now = time(NULL); 
    struct tm *tm_info = localtime(&now); 
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info); 
} 
 
/* ───────────────────────────────────────────── 
 * FEATURE 4 + 5: Log to file AND terminal 
 * ───────────────────────────────────────────── */ 
void log_event(const char *module, const char *event) { 
    char timestamp[32]; 
    get_timestamp(timestamp, sizeof(timestamp));
       /* Print to terminal with color */ 
    printf("%s[LOG] [%s] [%s] %s%s 
", 
           ANSI_CYAN, timestamp, module, event, ANSI_RESET); 
 
    /* Write to log file */ 
    FILE *fp = fopen(LOG_FILE, "a");   /* "a" = append mode */ 
    if (fp == NULL) { 
        printf("%s[LOGGER ERROR] Cannot open %s%s 
", 
               ANSI_YELLOW, LOG_FILE, ANSI_RESET); 
        return; 
    } 
    fprintf(fp, "[%s] [%-8s] %s 
", timestamp, module, event); 
    fclose(fp); 
} 
 
/* ───────────────────────────────────────────── 
 * FEATURE 6: View all log entries 
 * ───────────────────────────────────────────── */ 
void view_logs(void) { 
    FILE *fp = fopen(LOG_FILE, "r"); 
    char  line[256]; 
 
    printf(" 
%s========== IPC EVENT LOG ==========%s 
", 
           ANSI_GREEN, ANSI_RESET); 
 
    if (fp == NULL) { 
        printf("  No log file found. Run a demo first. 
"); 
        printf("%s====================================%s 
 
", 
               ANSI_GREEN, ANSI_RESET); 
        return; 
    } 
 
    int count = 0; 
    while (fgets(line, sizeof(line), fp)) { 
        printf("  %s", line); 
        count++; 
    } 
    fclose(fp); 
 
    if (count == 0) 
        printf("  Log file is empty. 
"); 
 
    printf("%s====================================%s 
", 
           ANSI_GREEN, ANSI_RESET); 
    printf("  Total entries: %d 
 
", count); 
} 
 
/* ───────────────────────────────────────────── 
 * FEATURE: Clear logs 
 * ───────────────────────────────────────────── */ 
void clear_logs(void) { 
    FILE *fp = fopen(LOG_FILE, "w");   /* "w" = overwrite / truncate */ 
    if (fp) { 
        fclose(fp); 
        printf("  Log file cleared. 
 
"); 
        log_event("SYSTEM", "Log file cleared by user"); 
    } else { 
        printf("  Could not clear log file. 
"); 
    } 
}