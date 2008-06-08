#include <sp.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

/* The default port to connect to */
#define SPLOGGER_PORT "4803"

#define BUFSIZE (8 * 1024)

int main(int argc, char **argv) {
	int c;
	char port[6] = SPLOGGER_PORT;
	char *group_name = NULL;

	opterr = 0;

	while ((c = getopt(argc, argv, "hg:p:")) != -1) {
		switch (c) {
			case 'g':
				group_name = malloc(MAX_GROUP_NAME);
				strncpy(group_name, optarg, MAX_GROUP_NAME - 1);
				break;
			case 'p':
				strncpy(port, optarg, 5);
				break;
			case '?':
				if (optopt == 'g')
					fprintf(stderr, "Option -g requires an argument.\n");
				else if (optopt == 'p')
					fprintf(stderr, "Option -p requires an argument.\n");
				else if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n",
							optopt);
				exit(EXIT_FAILURE);
			default:
				abort();
		}
	}

	if (group_name == NULL) {
		fprintf(stderr, "Error: you must specify a group with -g.\n");
		exit(EXIT_FAILURE);
	}

	int ret;
	mailbox mbox;
	char pgroupname[MAX_GROUP_NAME];

	/* Connect on 127.0.0.1:4803 */
	ret = SP_connect(port, NULL, 0, 0, &mbox, pgroupname);
	if (ret != ACCEPT_SESSION) {
		SP_error(ret);
		return -1;
	}

	/* Send out a message */
	char *msg = "hello world";

	int16 splogger_mess_type = 1; /* doesn't really do anything */

	char buf[BUFSIZE];
	/* FIXME: assumes stdin in 0 */
	while (1) {
		ret = read(0, buf, BUFSIZE);
		if (ret == 0)
			break;

		ret = SP_multicast(mbox, FIFO_MESS | SELF_DISCARD, group_name,
				splogger_mess_type, ret, buf);
	}

	/* Disconnect */
	ret = SP_disconnect(mbox);
	if (ret) {
		SP_error(ret);
		return -1;
	}
	return 0;
}
