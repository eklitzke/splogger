#include <sp.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.c"

#define SPLOGGER_MAX_GROUPS 10
#define MAX_MEMBERS     100
#define MAX_MESSLEN     102400

#define OPEN_FLAGS	(O_APPEND | O_CREAT | O_WRONLY)
#define OPEN_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

/* not re-entrant */
static char mess_buf[MAX_MESSLEN];
static mailbox mbox;
static char *group_name = NULL;
static char *config_name = NULL;

void shutdown_cleanly();
void splogger_signal(int signum);

int main(int argc, char **argv) {
	int c;
	int ret;
	int add_newlines = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "ng:c:")) != -1) {
		switch (c) {
			case 'g':
				group_name = malloc(MAX_GROUP_NAME); /* FIXME */
				strncpy(group_name, optarg, MAX_GROUP_NAME - 1);
				break;
			case 'n':
				add_newlines++;
				break;
			case 'c':
				config_name = strdup(optarg);
				break;
			case '?':
				if (optopt == 'g')
					fprintf(stderr, "Must pass a group name to the -g option.\n");
				else if (optopt == 'c')
					fprintf(stderr, "Must pass the path to a config file to the -c option.\n");
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

	if (config_name == NULL) {
		/* default to /tmp/sploggerd.conf */
		config_name = "/tmp/sploggerd.conf";
	}

	/* Now we need to validate the path to the config file */
	struct stat stat_buf;
	ret = stat(config_name, &stat_buf);
	if (ret == -1) {
		perror("stat()");
		exit(EXIT_FAILURE);
	}

	if (!S_ISREG(stat_buf.st_mode)) {
		fprintf("config file `%s' isn't a regular file or doesn't exit.\n", config_file);
		exit(EXIT_FAILURE);
	}

	/* Create the file with mask 664 */
	int log_fd = open("sploggerd.log", OPEN_FLAGS, OPEN_MODE);
	if (log_fd == -1) {
		perror("open()");
		exit(EXIT_FAILURE);
	}

	char pgroupname[MAX_GROUP_NAME];

	signal(SIGINT, splogger_signal);
	signal(SIGQUIT, splogger_signal);

	/* Connect on 127.0.0.1:4803 */
	ret = SP_connect("4803", NULL, 0, 0, &mbox, pgroupname);
	if (ret != ACCEPT_SESSION)
		splogger_fail(ret);

	/* Join the sploggerd group */
	ret = SP_join(mbox, group_name);
	if (ret != 0)
		splogger_fail(ret);

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
		if (bytes_recvd < 0)
			splogger_fail(bytes_recvd);

		/* Add a terminating newline character -- this lets us avoid an extra
		 * call to write(2) below */
		if (add_newlines) {
			mess_buf[bytes_recvd] = '\n';
			mess_buf[bytes_recvd + 1] = '\0'; /* FIXME */
		}
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
	ret1 = SP_leave(mbox, group_name);

	/* Disconnect */
	ret2 = SP_disconnect(mbox);
	if (ret2)
		splogger_fail(ret2);

	/* Only raise the failure for leaving the group now */
	if (ret1)
		splogger_fail(ret1);

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

void load_config() {
	int ret = open(config_name, O_RDONLY);
	if (ret < 0) {
		perror("open()");
		exit(EXIT_FAILURE);
	}

	int start, end;
	FILE *config_file = fopen(config_name, "r");

	size_t line_buf_size = 1024;
	char line_buf[line_buf_size];
	while (1) {
		start = 0;
		ret = getline(&line_buf, &line_buf_size, config_file);
		if (ret < 0) {
			perror("getline()");
			exit(EXIT_FAILURE);
		}
		/* Strip off comments and leading/trailing whitespace */
		printf("read line %s\n", line_buf);
	}
}
