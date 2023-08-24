SOURCES=get_ssl
PROGRAMS = $(SOURCES:.cc=)

default: $(PROGRAMS)

clean:
	rm *.o $(PROGRAMS) -f

.cc:
	g++ -Wall -g $< -o $@  -lssl -lcrypto