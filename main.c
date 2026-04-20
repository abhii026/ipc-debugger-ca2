#include <stdio.h>
#include <stdlib.h>
#include "module3_ui/ui_controller.h"

/* ============================================================
 * main.c — IPC Debugger Entry Point
 * Project : CA2 — Inter-Process Communication Debugger
 * ============================================================ */

int main(void) {
    /* Create logs/ directory if it doesn't exist */
    system("mkdir -p logs");

    /* Start the menu-driven UI */
    run_menu();

    return 0;
}
