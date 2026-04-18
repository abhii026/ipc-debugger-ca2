#include <stdio.h>
#include <stdlib.h>
#include "ui_controller.h"
#include "../module1_ipc/ipc_core.h"
#include "../module2_logger/logger.h"

/* ============================================================
 * ui_controller.c — Module 3: UI & Controller
 * ============================================================ */

/* ANSI Color Codes (FIXED) */
#define ANSI_BOLD "\033[1m"
#define ANSI_BLUE "\033[0;34m"
#define ANSI_RESET "\033[0m"

/* ─────────────────────────────────────────────
 * FEATURE 7: Main Menu Display
 * ───────────────────────────────────────────── */
static void print_menu(void) {
    printf("\n");
    printf("%s╔══════════════════════════════════════╗%s\n", ANSI_BLUE, ANSI_RESET);
    printf("%s║ IPC DEBUGGER — CA2 Project         ║%s\n", ANSI_BLUE, ANSI_RESET);
    printf("%s╠══════════════════════════════════════╣%s\n", ANSI_BLUE, ANSI_RESET);
    printf("%s║%s 1. Test Pipe Communication        %s║%s\n", ANSI_BLUE, ANSI_RESET, ANSI_BLUE, ANSI_RESET);
    printf("%s║%s 2. Test Shared Memory             %s║%s\n", ANSI_BLUE, ANSI_RESET, ANSI_BLUE, ANSI_RESET);
    printf("%s║%s 3. Test Signal Handling           %s║%s\n", ANSI_BLUE, ANSI_RESET, ANSI_BLUE, ANSI_RESET);
    printf("%s║%s 4. View Debug Logs                %s║%s\n", ANSI_BLUE, ANSI_RESET, ANSI_BLUE, ANSI_RESET);
    printf("%s║%s 5. Clear Logs                    %s║%s\n", ANSI_BLUE, ANSI_RESET, ANSI_BLUE, ANSI_RESET);
    printf("%s║%s 6. About / Help                  %s║%s\n", ANSI_BLUE, ANSI_RESET, ANSI_BLUE, ANSI_RESET);
    printf("%s║%s 7. Exit                          %s║%s\n", ANSI_BLUE, ANSI_RESET, ANSI_BLUE, ANSI_RESET);
    printf("%s╚══════════════════════════════════════╝%s\n", ANSI_BLUE, ANSI_RESET);
    printf("Enter your choice (1-7): ");
}

/* ─────────────────────────────────────────────
 * FEATURE 9: About / Help Screen
 * ───────────────────────────────────────────── */
void show_about(void) {
    printf("\n%s╔════════════ ABOUT ════════════╗%s\n", ANSI_BLUE, ANSI_RESET);
    printf(" %sIPC Debugger — CA2 Project%s\n", ANSI_BOLD, ANSI_RESET);
    printf(" Language : C (C99)\n");
    printf(" Platform : Linux / WSL2\n");

    printf("\n %sIPC Mechanisms:%s\n", ANSI_BOLD, ANSI_RESET);
    printf(" 1. Pipes — Unidirectional parent-child communication\n");
    printf(" 2. Shared Memory — Two processes share same memory\n");
    printf(" 3. Signals — Processes send alerts\n");

    printf("\n %sTeam Members:%s\n", ANSI_BOLD, ANSI_RESET);
    printf(" Member 1: IPC Core Engine (ipc_core.c)\n");
    printf(" Member 2: Logger / Debugger (logger.c)\n");
    printf(" Member 3: UI Controller (ui_controller.c)\n");

    printf("%s╚════════════════════════════════╝%s\n", ANSI_BLUE, ANSI_RESET);
}

/* ─────────────────────────────────────────────
 * FEATURE 8: Input Validation + Controller
 * ───────────────────────────────────────────── */
void run_menu(void) {
    int choice;

    log_event("SYSTEM", "IPC Debugger started");

    while (1) {
        print_menu();

        /* Input Validation */
        if (scanf("%d", &choice) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            printf("\n[ERROR] Invalid input. Please enter a number (1-7).\n");
            continue;
        }

        printf("\n");

        switch (choice) {

            case 1:
                printf("\n--- PIPE DEMO ---\n");
                log_event("INFO", "Running Pipe Demo");
                run_pipe_demo();
                break;

            case 2:
                printf("\n--- SHARED MEMORY DEMO ---\n");
                log_event("INFO", "Running Shared Memory Demo");
                run_shared_memory_demo();
                break;

            case 3:
                printf("\n--- SIGNAL DEMO ---\n");
                log_event("INFO", "Running Signal Demo");
                run_signal_demo();
                break;

            case 4:
                printf("\n--- VIEW LOGS ---\n");
                view_logs();
                break;

            case 5:
                printf("\n--- CLEAR LOGS ---\n");
                clear_logs();
                break;

            case 6:
                show_about();
                break;

            case 7:
                printf("\nExiting IPC Debugger. Goodbye!\n");
                log_event("SYSTEM", "IPC Debugger exited by user");
                return;

            default:
                printf("\n[ERROR] Choice must be between 1 and 7.\n");
        }
    }
}