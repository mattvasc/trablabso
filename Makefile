CC = gcc
CFLAGS = -Wall -g

OBJS = disk.o shell.o fs.o

rsfs: $(OBJS)
	$(CC) -o rsfs $(OBJS)

disk.o: disk.h
fs.o: fs.h disk.h
shell.o: disk.h fs.h

.PHONY : clean run
clean:
	rm -f *.o *~ rsfs hd.lbso
run: rsfs
	./rsfs hd.lbso 2
