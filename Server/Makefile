# /server/Makefile
# by Louis Nguyen, Leo Deng, Hersh
# nguye576,...

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = musas

all: $(TARGET)

$(TARGET): musas.o
	$(CC) $(CFLAGS) -o $(TARGET) musas.o

musas.o: musas.c
	$(CC) $(CFLAGS) -c musas.c

clean:
	rm -f $(TARGET) *.o
