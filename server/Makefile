# Makefile for aesdsocket program

# Compiler and compilation flags
CC ?= gcc
CFLAGS ?= -Wall -Werror

# Cross compilation support
CROSS_COMPILE ?=

# Target executable
TARGET = aesdsocket

# Source files
SRCS = aesdsocket.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $^ -pthread

# Rule to build object files
%.o: %.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c -o $@ $<

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

