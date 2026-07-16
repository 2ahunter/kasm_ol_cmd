objects = UDP_client.o main.o

CFLAGS = -Wall -Wextra -pedantic -std=gnu17

# $@ is the target, $^ are the prerequisites
cmd_kasm: $(objects)
	cc -o $@ $^ $(LDLIBS)

main.o: main.c UDP_client.o

UDP_client.o: UDP_client.c UDP_client.h

.PHONY : clean
clean :
	rm cmd_kasm $(objects)