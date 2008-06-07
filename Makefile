splogger: splogger.c
	LD_RUN_PATH=$$HOME/local/lib gcc -I$$HOME/local/include -L$$HOME/local/lib -lspread -o splogger splogger.c
