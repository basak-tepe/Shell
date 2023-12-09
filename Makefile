CC = gcc
CFLAGS = -Wall -Wextra -pedantic

myshell: myshell.c
	$(CC) $(CFLAGS) -o myshell myshell.c

clean:
	rm -f myshell


