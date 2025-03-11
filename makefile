CC = gcc

OBJCS = client4.c 
OBJCSS = server4.c

CFLAGS = -g -Wall
LIBS = -lcjson -lssl -lcrypto

all: client4 server4

client4: $(OBJCS)
	$(CC) $(CFLAGS) -o $@ $(OBJCS) $(LIBS)

server4: $(OBJCSS)
	$(CC) $(CFLAGS) -o $@ $(OBJCSS) $(LIBS)

clean:
	rm -f client4 server4
