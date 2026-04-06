# /client/Makefile
# by Louis Nguyen, Leo Deng, Hersh
# PU handles: nguye576,...

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = musac

all: $(TARGET)

$(TARGET): musac.o
	$(CC) $(CFLAGS) -o $(TARGET) musac.o -lasound -lm

musac.o: musac.c
	$(CC) $(CFLAGS) -c musac.c

clean:
	rm -f $(TARGET) *.o
