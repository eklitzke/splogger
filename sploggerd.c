#define _GNU_SOURCE /* Use GNU extensions, like GNU getline */

#include <sp.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "data.h"
#include "code_config.h"

/* not re-entrant */
static char mess_buf[MAX_MESSLEN];
static mailbox mbox;
static char *group_name = NULL;

void shutdown_cleanly(int dummy);

void splogger_fail(int ret) {
	SP_error(ret);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	config_name = NULL;

	int c, i;
	int ret;
	int add_newlines = 0;
	int daemon = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "hng:c:d")) != -1) {
		switch (c) {
			case 'h':
				printf("Usage: splogger -g group_name [-h] [-d] [-f rule_file] [-c config_file]\n");
				printf(" -h   print this help text\n");
				printf(" -f   use this file for code/name rules\n");
				printf(" -c   use this configuration file\n");
				printf(" -d   run sploggerd as a daemon\n");
				exit(EXIT_SUCCESS);
				break;
			case 'g':
				group_name = malloc(MAX_GROUP_NAME); /* FIXME */
				strncpy(group_name, optarg, MAX_GROUP_NAME - 1);
				break;
			case 'n':
				add_newlines++;
				break;
			case 'f':
				config_name = strdup(optarg);
				break;
			case 'd':
				daemon++;
				break;
			case '?':
				if (optopt == 'g')
					fprintf(stderr, "Must pass a group name to the -g option.\n");
				else if (optopt == 'c')
					fprintf(stderr, "Must pass the path for a config file to the -c option.\n");
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

	/* Initialize the code tables */
	for (i = 0; i < MAX_CODE; i++)
		code_table[i] = NULL;

	/* Now we need to validate the path to the config file */
	struct stat stat_buf;
	ret = stat(config_name, &stat_buf);
	if (ret == -1) {
		if (errno == ENOENT)
			fprintf(stderr, "Config file %s does not exist!\n", config_name);
		else
			perror("stat()");
		exit(EXIT_FAILURE);
	}

	if (!S_ISREG(stat_buf.st_mode)) {
		fprintf(stderr, "config file `%s' isn't a regular file.\n", config_name);
		exit(EXIT_FAILURE);
	}

	/* Load the configuration settings */
	load_code_config(0);

	char pgroupname[MAX_GROUP_NAME];

	signal(SIGHUP, load_code_config);
	signal(SIGINT, shutdown_cleanly);
	signal(SIGQUIT, shutdown_cleanly);
	signal(SIGTERM, shutdown_cleanly);

	/* Now it's safe to daemonize. We want to do this after loading up the
	 * configuration so we can print parse errors and the like. */

	if (daemon) {
		pid_t pid;
		pid = fork();
		if (pid < 0) {
			perror("fork()");
			exit(EXIT_FAILURE);
		} else if (pid > 0)
			exit(EXIT_SUCCESS);

		umask(0);
		if (setsid() < 0) {
			perror("setsid()");
			exit(EXIT_FAILURE);
		}
		if (chdir("/") < 0) {
			perror("chdir()");
			exit(EXIT_FAILURE);
		}
		close(0); close(1); close(2);
	}

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
		fprintf(stderr, "got a message\n");

		/* Handle error conditions */
		if (bytes_recvd < 0)
			splogger_fail(bytes_recvd);

		if (mess_type >= MAX_CODE) {
			fprintf(stderr, "Got unexpected code %d.\n", mess_type);
			exit(EXIT_FAILURE);
		}

		file_tbl *ent = code_table[mess_type];
		if (ent == NULL) {
			/* FIXME: use the logging facility once it's in place */
			fprintf(stderr, "Code %d does not exit in code table.\n", mess_type);
		}

		/* Add a terminating newline character -- this lets us avoid an extra
		 * call to write(2) below */
		if (add_newlines) {
			mess_buf[bytes_recvd] = '\n';
			mess_buf[bytes_recvd + 1] = '\0'; /* FIXME */
		}
		bytes_recvd++;

		int bytes, written = 0;
		while (bytes_recvd - written > 0) {
			if ((written == 0) && (ent == NULL)) {
				char code_string[6];
				if (sprintf(code_string, "%d ", mess_type) < 0) {
					perror("sprintf()");
					exit(EXIT_FAILURE);
				}
				bytes = write(unknown_fd, code_string, strlen(code_string));
				if (bytes < 0) {
					perror("write()");
					exit(EXIT_FAILURE);
				}
			}
			bytes = write((ent == NULL) ? unknown_fd : ent->fd, mess_buf +
					written, bytes_recvd - written);
			if (bytes < 0) {
				perror("write()");
				exit(EXIT_FAILURE);
			}
			written += bytes;
		}
	}

	assert(0); /* Not reached */
}

void shutdown_cleanly(int signal) {
	/* Leave the group */
	int ret1, ret2;
	ret1 = SP_leave(mbox, group_name);

	/* Disconnect */
	ret2 = SP_disconnect(mbox);
	if (ret2)
		splogger_fail(ret2);

	/* Only raise the failure for leaving the group now */
	if (ret1)
		splogger_fail(ret1);

	switch (signal) {
		case SIGQUIT:
			exit(EXIT_FAILURE);
		default:
			exit(EXIT_SUCCESS);
	}
}
