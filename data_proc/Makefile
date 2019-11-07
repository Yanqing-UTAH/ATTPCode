.PHONY: all clean

all: proc_world_cup

proc_world_cup: proc_world_cup.o ../external/ita_public_tools/endian.o
	g++ -o $@ $^

proc_world_cup.o: proc_world_cup.cpp
	g++ -c -O2 -o $@ -std=gnu++17 -I../external/ita_public_tools/src $<

../external/ita_public_tools/endian.o:
	make -C ../external/ita_public_tools endian.o

clean:
	rm -f proc_world_cup proc_world_cup.o

