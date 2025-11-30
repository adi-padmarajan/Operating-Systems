CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: diskinfo disklist diskget diskput

diskinfo: parts.c
	$(CC) $(CFLAGS) -DDISKINFO -o diskinfo parts.c

disklist: parts.c
	$(CC) $(CFLAGS) -DDISKLIST -o disklist parts.c

diskget: parts.c
	$(CC) $(CFLAGS) -DDISKGET -o diskget parts.c

diskput: parts.c
	$(CC) $(CFLAGS) -DDISKPUT -o diskput parts.c

clean:
	rm -f diskinfo disklist diskget diskput
