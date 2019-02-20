# Target library
lib := libuthread.a
objs := queue.o thread.o sem.o
CC := gcc
CFLAGS := -Wall -Werror

all: $(lib)

$(lib): $(objs)
	ar rcs $(lib) $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(lib) sem.o
