CC = gcc
CFLAGS = -Wall

SRC = main.c \
      module1_ipc/ipc_core.c \
      module2_logger/logger.c \
      module3_ui/ui_controller.c

OUT = main

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)