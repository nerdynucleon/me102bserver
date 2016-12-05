all: main.o
	g++ -o server main.o
main.o: main.cpp
	g++ -c main.cpp
clean: main.o server
	rm main.o server
