#ifndef _SPLOGGER_DATA_H_
#define _SPLOGGER_DATA_H_

#define SPLOGGER_MAX_GROUPS 10
#define MAX_MEMBERS         100
#define MAX_MESSLEN         102400

/* Must be a power of 2 */
#define MAX_CODE            256

/* Parse error codes */
#define PARSE_ERR_GEN    0x01 /* generic error (e.g. if getline() fails) */
#define PARSE_ERR_FILE   0x02 /* couldn't open the log file */
#define PARSE_ERR_CODE   0x04 /* couldn't parse the code for the line */
#define PARSE_ERR_LOGN   0x08 /* rule didn't have a log name */
#define PARSE_ERR_RESERVED 0x10 /* used a reserved log name */

#define ADD_PERR(x, c) (x | (c << 16))

#define OPEN_FLAGS	(O_APPEND | O_CREAT | O_WRONLY)
#define OPEN_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

#define SPLOGGER_UNKNOWN_LOG "unknown.splog"

struct file_tbl_s {
	char *fname;
	int fd;
};

typedef struct file_tbl_s file_tbl;

char *config_name;

#endif /* _SPLOGGER_DATA_H_ */
