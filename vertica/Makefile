.PHONY: all
CXXFLAGS=
LDFLAGS=-lodbc

all: scalability_test_client_id

scalability_test_client_id: scalability_test_client_id.cpp
	g++ -std=gnu++17 -O2 -fno-strict-aliasing -o $@ $(CXXFLAGS) $< $(LDFLAGS)

clean:
	rm -f scalability_test_client_id
