#include <sp.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SPLOGGER_MAX_GROUPS 10
#define MAX_MEMBERS     100
#define MAX_MESSLEN     102400

/* not re-entrant */
char mess_buf[MAX_MESSLEN];

int main(int argc, char **argv) {

	int log_fd = open("sploggerd.log", O_APPEND | O_CREAT | O_TRUNC |
			O_WRONLY);

	int ret;
	mailbox mbox;
	char pgroupname[MAX_GROUP_NAME];

	/* Connect on 127.0.0.1:4803 */
	ret = SP_connect("4803", NULL, 0, 0, &mbox, pgroupname);
	if (ret != ACCEPT_SESSION) {
		SP_error(ret);
		exit(EXIT_FAILURE);
	}

	/* Join the sploggerd group */
	ret = SP_join(mbox, "sploggerd");
	if (ret != 0) {
		SP_error(ret);
		exit(EXIT_FAILURE);
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
			SP_error(bytes_recvd);
			exit(EXIT_FAILURE);
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

	/* Leave the sploggerd group */
	ret = SP_leave(mbox, "sploggerd");
	if (ret != 0) {
		SP_error(ret);
		exit(EXIT_FAILURE);
	}

	/* Disconnect */
	ret = SP_disconnect(mbox);
	if (ret) {
		SP_error(ret);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
