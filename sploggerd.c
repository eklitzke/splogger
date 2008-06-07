#include <sp.h>
#include <stdio.h>

#define SPLOGGER_MAX_GROUPS 10
#define MAX_MEMBERS     100
#define MAX_MESSLEN     102400

/* not re-entrant */
char mess_buf[MAX_MESSLEN];

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

	int service_type = 0;
	int num_groups;
	char target_groups[MAX_MEMBERS][MAX_GROUP_NAME];

	char sender[MAX_GROUP_NAME];
	int16 mess_type;
	int endian_mismatch;

	ret = SP_receive(mbox, (service *) &service_type, sender,
			SPLOGGER_MAX_GROUPS, &num_groups, target_groups, &mess_type,
			&endian_mismatch, MAX_MESSLEN, mess_buf);
	printf("ret = %d\n", ret);

	/* Disconnect */
	ret = SP_disconnect(mbox);
	if (ret) {
		SP_error(ret);
		return -1;
	}
	return 0;
}
