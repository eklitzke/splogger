all: splogger_msg sploggerd

splogger_msg: splogger_msg.c
	LD_RUN_PATH=$$HOME/local/lib gcc -Os -I$$HOME/local/include -L$$HOME/local/lib -lspread -o splogger_msg splogger_msg.c

sploggerd: sploggerd.c
	LD_RUN_PATH=$$HOME/local/lib gcc -Os -I$$HOME/local/include -L$$HOME/local/lib -lspread -o sploggerd sploggerd.c
