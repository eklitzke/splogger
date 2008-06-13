#ifndef _SPLOGGER_CODE_CONFIG_H_
#define _SPLOGGER_CODE_CONFIG_H_

#include "data.h"

file_tbl* code_table[MAX_CODE];

/* The file descriptor for our "unknown code" log */
int unknown_fd;

void load_code_config(int dummy);

#endif /* _SPLOGGER_CODE_CONFIG_H_ */
