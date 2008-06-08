all: splogger_msg sploggerd

clean:
	-rm -f sploggerd splogger_msg

splogger_msg: splogger_msg.c
	LD_RUN_PATH=$$HOME/local/lib gcc -g -Os -I$$HOME/local/include -L$$HOME/local/lib -lspread -o splogger_msg splogger_msg.c

sploggerd: sploggerd.c
	LD_RUN_PATH=$$HOME/local/lib gcc -g -Os -I$$HOME/local/include -L$$HOME/local/lib -lspread -o sploggerd sploggerd.c
