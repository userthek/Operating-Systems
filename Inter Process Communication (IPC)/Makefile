CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread
TARGET = process_simulation
SRCS = parent.c child.c common.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
