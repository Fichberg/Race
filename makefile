race: race.o
	gcc -pthread -o race race.o

race.o: race.c
	gcc -c race.c -Wall -pedantic -ansi -g

clean:
	rm -rf *.o
	rm -rf *~
	rm race