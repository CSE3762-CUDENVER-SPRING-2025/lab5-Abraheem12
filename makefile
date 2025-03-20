CC = gcc

OBJCS = client5.c 
OBJCSS = server5.c

CFLAGS = -g -Wall
LIBS = -lcjson -lssl -lcrypto

all: client5 server5

client5: $(OBJCS)
	$(CC) $(CFLAGS) -o $@ $(OBJCS) $(LIBS)

server5: $(OBJCSS)
	$(CC) $(CFLAGS) -o $@ $(OBJCSS) $(LIBS)

clean:
	rm -f client5 server5
