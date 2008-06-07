#ifndef _SPLOGGER_UTIL_C_
#define _SPLOGGER_UTIL_C_

void splogger_fail(int ret) {
	SP_error(ret);
	exit(EXIT_FAILURE);
}

#endif /* _SPLOGGER_UTIL_C_ */
