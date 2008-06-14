CFLAGS=-Wall -g -Os
LDFLAGS=-lspread -L$$HOME/local/lib

all: sploggerd splogger_msg

clean:
	-rm -f sploggerd splogger_msg *.o

code_config.o: code_config.h code_config.c
	gcc $(CFLAGS) -c code_config.c

splogger_msg: splogger_msg.c
	gcc $(CFLAGS) $(LDFLAGS) -o splogger_msg splogger_msg.c

sploggerd: sploggerd.c code_config.o
	gcc $(CFLAGS) $(LDFLAGS) -o sploggerd sploggerd.c code_config.o
