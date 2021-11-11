.PHONY: all clean

all: create_client_ip_log \
	create_client_id_log \
	create_object_id_log \
	create_scalability_test_client_id \
	create_scalability_test_client_id_for_vertica

create_client_ip_log: create_client_ip_log.o ../../external/ita_public_tools/endian.o
	g++ -o $@ $^

create_client_ip_log.o: create_client_ip_log.cpp
	g++ -c -O2 -o $@ -std=gnu++17 -I../../external/ita_public_tools/src $<

create_client_id_log: create_client_id_log.o ../../external/ita_public_tools/endian.o
	g++ -o $@ $^

create_client_id_log.o: create_client_id_log.cpp
	g++ -c -O2 -o $@ -std=gnu++17 -I../../external/ita_public_tools/src $<

create_object_id_log: create_object_id_log.o ../../external/ita_public_tools/endian.o
	g++ -o $@ $^

create_object_id_log.o: create_object_id_log.cpp
	g++ -c -O2 -o $@ -std=gnu++17 -I../../external/ita_public_tools/src $<

../../external/ita_public_tools/endian.o:
	make -C ../../external/ita_public_tools endian.o

create_scalability_test_client_id.o: create_scalability_test_client_id.cpp
	g++ -c -O2 -o $@ -std=gnu++17 -I../../external/ita_public_tools/src $<

create_scalability_test_client_id: create_scalability_test_client_id.o ../../external/ita_public_tools/endian.o
	g++ -o $@ $^

create_scalability_test_client_id_for_vertica.o: create_scalability_test_client_id_for_vertica.cpp
	g++ -c -O2 -o $@ -std=gnu++17 -I../../external/ita_public_tools/src $<

create_scalability_test_client_id_for_vertica: create_scalability_test_client_id_for_vertica.o ../../external/ita_public_tools/endian.o
	g++ -o $@ $^

clean:
	rm -f create_client_ip_log create_client_ip_log.o \
		create_client_id_log create_client_id_log.o \
		create_object_id_log create_object_id_log.o \
		create_scalability_test_client_id create_scalability_test_client_id.o \
		create_scalability_test_client_id_for_vertica \
		create_scalability_test_client_id_for_vertica.o

