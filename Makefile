CFLAG = -Wall -O2 -std=c++11

detection: detection.cpp psketch.o pla.o
	g++ $(CFLAG) detection.cpp psketch.o pla.o -o detection
psketch.o: psketch.cpp psketch.h
	g++ -c $(CFLAG) -o psketch.o psketch.cpp
pla.o: pla.cpp pla.h
	g++ -c $(CFLAG) -o pla.o pla.cpp
clean:
	rm -f *.o detection

