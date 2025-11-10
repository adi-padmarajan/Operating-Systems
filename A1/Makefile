all: sample inf args ssi

sample: sample.c
	gcc sample.c -lreadline -lhistory -ltermcap -o sample

inf: inf.c
	gcc inf.c -o inf

args: args.c
	gcc args.c -o args

clean:
	rm -f *.o
	rm -f sample
	rm -f inf
	rm -f args

#Makefile segment for assignment 1
ssi: ssi.c
	gcc ssi.c -std=c11 -Wall -Wextra -g -lreadline -lncurses -o ssi