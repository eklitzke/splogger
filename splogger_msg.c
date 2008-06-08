#include <sp.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/* The default port to connect to */
#define SPLOGGER_PORT "4803"

#define BUFSIZE (16 * 1024)

int main(int argc, char **argv) {
	int c;
	char port[6] = SPLOGGER_PORT;
	char *group_name = NULL;
	int16 code = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "hg:p:c:")) != -1) {
		switch (c) {
			case 'g':
				group_name = malloc(MAX_GROUP_NAME);
				strncpy(group_name, optarg, MAX_GROUP_NAME - 1);
				break;
			case 'p':
				strncpy(port, optarg, 5);
				break;
			case 'c':
				code = (int16) atoi(optarg);
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

	/* Open the file */
	int input_fd = 0; /* Default is stdin */
	if (optind < argc) {
		input_fd = open(argv[optind], O_RDONLY);
		if (input_fd < 0) {
			perror("open()");
			exit(EXIT_FAILURE);
		}
	}

	/* Read in the buffer */
	int bytes_read = 0;
	char buf[BUFSIZE];
	while (1) {
		ret = read(input_fd, buf + bytes_read, BUFSIZE - bytes_read);
		if (ret == 0) {
			close(input_fd);
			break;
		}
		bytes_read += ret;

	}

	/* Send out the message */
	ret = SP_multicast(mbox, FIFO_MESS | SELF_DISCARD, group_name, code,
			bytes_read, buf);

	/* Disconnect */
	ret = SP_disconnect(mbox);
	if (ret) {
		SP_error(ret);
		return -1;
	}
	return 0;
}
