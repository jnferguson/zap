CC=/usr/bin/g++
RM=/usr/bin/rm

all:
	$(CC) -std=c++11 -Wall -Werror -pedantic -c siphash.cpp -o siphash.o
	$(CC) -std=c++11 -Wall -Werror -pedantic -c lookup3.cpp -o lookup3.o
	$(CC) -std=c++11 -Wall -Werror -pedantic -c log.cpp -o log.o
	$(CC) -std=c++11 -Wall -Werror -pedantic -c object.cpp -o object.o
	$(CC) -std=c++11 -Wall -Werror -pedantic -c journal.cpp -o journal.o
	$(CC) -std=c++11 -Wall -Werror -pedantic -c input_journal.cpp -o input_journal.o
	$(CC) -std=c++11 -Wall -Werror -pedantic -c output_journal.cpp -o output_journal.o
	$(CC) -std=c++11 -Wall -Werror -pedantic -c file.cpp -o file.o
	$(CC) -std=c++11 -Wall -Werror -pedantic -c main.cpp -o main.o
	$(CC) -o zap main.o file.o object.o journal.o input_journal.o output_journal.o log.o siphash.o lookup3.o

clean:
	$(RM) -f zap main.o file.o object.o journal.o input_journal.o output_journal.o log.o siphash.o lookup3.o

