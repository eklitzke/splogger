PREFIX=$$HOME/local
CFLAGS=-Wall -g -Os -I$(PREFIX)/include -L$(PREFIX)/lib
LDFLAGS=-lspread

all: sploggerd splogger_msg

clean:
	-rm -f sploggerd splogger_msg *.o

code_config.o: code_config.h code_config.c
	gcc $(CFLAGS) -c code_config.c

splogger_msg: splogger_msg.c
	LD_RUN_PATH=$$HOME/local/lib gcc $(CFLAGS) $(LDFLAGS) -o splogger_msg splogger_msg.c

sploggerd: sploggerd.c code_config.o
	LD_RUN_PATH=$$HOME/local/lib gcc $(CFLAGS) $(LDFLAGS) -o sploggerd sploggerd.c code_config.o
