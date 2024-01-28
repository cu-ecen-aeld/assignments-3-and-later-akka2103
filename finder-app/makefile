# Default compiler and compiler flags
CC := gcc
CF := -Wall

# If Cross Compiler provided as argument
ifdef CROSS_COMPILE
	CC := $(CROSS_COMPILE)gcc
endif

# Executable
OUTPUT := writer

# Default Target
$(OUTPUT): writer.o
	$(CC) $(CF) writer.o -o $(OUTPUT)

writer.o: writer.c
	$(CC) $(CF) -c writer.c

clean:
	rm -f *.o $(OUTPUT)

