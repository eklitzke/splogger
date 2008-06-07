#include <sp.h>

#define SPLOGGER_PORT "4803"

int main(int argc, char **argv) {
	int ret;
	mailbox mbox;
	char pgroupname[MAX_GROUP_NAME];

	/* Connect on 127.0.0.1:4803 */
	ret = SP_connect(SPLOGGER_PORT, NULL, 0, 0, &mbox, pgroupname);
	if (ret != ACCEPT_SESSION) {
		SP_error(ret);
		return -1;
	}

	/* Send out a message */
	char *msg = "hello world";

	/* Disconnect */
	ret = SP_disconnect(mbox);
	if (ret) {
		SP_error(ret);
		return -1;
	}
	return 0;
}
