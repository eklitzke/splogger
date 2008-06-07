#include <sp.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.c"

#define SPLOGGER_MAX_GROUPS 10
#define MAX_MEMBERS     100
#define MAX_MESSLEN     102400

/* not re-entrant */
char mess_buf[MAX_MESSLEN];
mailbox mbox;

void shutdown_cleanly();
void splogger_signal(int signum);

int main(int argc, char **argv) {

	/* Create the file with mask 664 */
	int log_fd = open("sploggerd.log", O_APPEND | O_CREAT | O_TRUNC | O_WRONLY,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (log_fd == -1) {
		perror("open()");
		exit(EXIT_FAILURE);
	}

	int ret;
	char pgroupname[MAX_GROUP_NAME];

	signal(SIGINT, splogger_signal);
	signal(SIGQUIT, splogger_signal);

	/* Connect on 127.0.0.1:4803 */
	ret = SP_connect("4803", NULL, 0, 0, &mbox, pgroupname);
	if (ret != ACCEPT_SESSION) {
		splogger_fail(ret);
	}

	/* Join the sploggerd group */
	ret = SP_join(mbox, "sploggerd");
	if (ret != 0) {
		splogger_fail(ret);
	}

	int service_type = 0;
	int num_groups;
	char target_groups[MAX_MEMBERS][MAX_GROUP_NAME];

	char sender[MAX_GROUP_NAME];
	int16 mess_type;
	int endian_mismatch;

	/* The main loop where we receive data and then write it out to the log
	 * file. */
	while (1) {
		int bytes_recvd = SP_receive(mbox, (service *) &service_type, sender,
				SPLOGGER_MAX_GROUPS, &num_groups, target_groups, &mess_type,
				&endian_mismatch, MAX_MESSLEN - 1, mess_buf);
		if (bytes_recvd < 0) {
			splogger_fail(bytes_recvd);
		}

		/* Add a terminating newline character -- this lets us avoid an extra
		 * call to write(2) below */
		mess_buf[bytes_recvd] = '\n';
		bytes_recvd++;

		int written = 0;
		while (bytes_recvd - written > 0) {
			int bytes = write(log_fd, mess_buf + written, bytes_recvd - written);
			if (bytes < 0) {
				perror("write()");
				exit(EXIT_FAILURE);
			}
			written += bytes;
		}
	}

	shutdown_cleanly();

}

void shutdown_cleanly() {
	/* Leave the sploggerd group */
	int ret1, ret2;
	ret1 = SP_leave(mbox, "sploggerd");

	/* Disconnect */
	ret2 = SP_disconnect(mbox);
	if (ret2) {
		splogger_fail(ret2);
	}

	/* Only raise the failure for leaving the group now */
	if (ret1) {
		splogger_fail(ret1);
	}

	exit(EXIT_SUCCESS);
}

void splogger_signal(int signum) {
	switch(signum) {
		case SIGQUIT:
		case SIGINT:
			shutdown_cleanly();
			break;
		default:
			fprintf(stderr, "Got unknown signal %d\n", signum);
			exit(EXIT_FAILURE);
	}
}
