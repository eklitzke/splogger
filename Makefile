PREFIX=$$HOME/local
CFLAGS=-Wall -g -Os -I$(PREFIX)/include -L$(PREFIX)/lib
LDFLAGS=-lspread

all: splogger_msg sploggerd

clean:
	-rm -f sploggerd splogger_msg

splogger_msg: splogger_msg.c
	LD_RUN_PATH=$$HOME/local/lib gcc $(CFLAGS) $(LDFLAGS) -o splogger_msg splogger_msg.c

sploggerd: sploggerd.c
	LD_RUN_PATH=$$HOME/local/lib gcc $(CFLAGS) $(LDFLAGS) -o sploggerd sploggerd.c
