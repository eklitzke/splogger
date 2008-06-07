#include <sp.h>

/* I *think* this is in seconds but it's not clear from the API */
#ifndef TCP_TIMEOUT
#define TCP_TIMEOUT 5
#endif

int main(int argc, char **argv) {
	int ret;
	mailbox mbox;
	char pgroupname[MAX_GROUP_NAME + 1]; /* not sure if i need space for the terminating null... */

	/* Connect on 127.0.0.1:4803 */
	ret = SP_connect("4803", NULL, 0, 0, &mbox, pgroupname);
	if (ret != ACCEPT_SESSION) {
		SP_error(ret);
		return -1;
	}
	return 0;
}
