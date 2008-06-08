#include <sp.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.c"
#include "data.h"

#define SPLOGGER_MAX_GROUPS 10
#define MAX_MEMBERS         100
#define MAX_MESSLEN         102400
#define MAX_CODE            256

#define OPEN_FLAGS	(O_APPEND | O_CREAT | O_WRONLY)
#define OPEN_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

/* not re-entrant */
static char mess_buf[MAX_MESSLEN];
static mailbox mbox;
static char *group_name = NULL;
static char *config_name = NULL;

static (file_tbl *) code_table[MAX_CODE];

void shutdown_cleanly();
void splogger_signal(int signum);
void load_config();

int main(int argc, char **argv) {
	int c, i;
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

	/* Initialize the code tables */
	for (i = 0; i < MAX_CODE; i++)
		code_table[i] = NULL;

	/* Load the configuration settings */
	load_config();

	/* Now we need to validate the path to the config file */
	struct stat stat_buf;
	ret = stat(config_name, &stat_buf);
	if (ret == -1) {
		perror("stat()");
		exit(EXIT_FAILURE);
	}

	if (!S_ISREG(stat_buf.st_mode)) {
		fprintf(stderr, "config file `%s' isn't a regular file or doesn't exit.\n", config_name);
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

		/* Handle error conditions */
		if (bytes_recvd < 0)
			splogger_fail(bytes_recvd);

		if (mess_type >= MAX_CODE) {
			fprintf(stderr, "Got unexpected code %d.\n", mess_type);
			exit(EXIT_FAILURE);
		}

		file_tbl *ent = code_table[mess_type];
		if (ent == NULL) {
			fprintf(stderr, "Code %d does not exit in code table.\n", code);
			continue;
		}

		/* Add a terminating newline character -- this lets us avoid an extra
		 * call to write(2) below */
		if (add_newlines) {
			mess_buf[bytes_recvd] = '\n';
			mess_buf[bytes_recvd + 1] = '\0'; /* FIXME */
		}
		bytes_recvd++;

		int written = 0;
		while (bytes_recvd - written > 0) {
			int bytes = write(ent->fd, mess_buf + written, bytes_recvd - written);
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
		case SIGHUP:
			load_config();
			break;
		default:
			fprintf(stderr, "Got unknown signal %d\n", signum);
			exit(EXIT_FAILURE);
	}
}

/* This runs once when the program starts -- it's also the routine called when
 * the program receives a SIGHUP.
 *
 * TODO: Report parse errors better, try not to bail out in this function
 */
void load_config() {
	int ret = open(config_name, O_RDONLY);
	int i;
	if (ret < 0) {
		perror("open()");
		exit(EXIT_FAILURE);
	}

	/* Clear out the old table */
	for (i = 0; i < MAX_CODE; i++) {
		file_tbl *ent = code_table[i];
		if (file_tbl == NULL)
			continue;
		close(file_tbl->fd);
		free(file_tble->fname);
	}

	int start, end;
	FILE *config_file = fopen(config_name, "r");

	size_t buf_size = 0;
	char *line_buf = NULL;
	while (1) {
		start = 0;
		printf("reading line...\n");
		ret = getline(&line_buf, &buf_size, config_file);
		if (ret == -1) {
			perror("getline()");
			exit(EXIT_FAILURE);
		}

		/* Strip initial whitespace */
		for (start = 0; isspace(line_buf[start]); start++);
		if (line_buf[start] == '\0')
			continue;

		/* Strip the comment if present */
		for (end = start; (end < buf_size) && (line_buf[end] != '#'); end++);

		/* Strip trailing whitespace */
		for (; (end > start) && isspace(line_buf[end]); end++);
		if (end == start)
			continue;

		/* Null terminate the string */
		line_buf[end] = '\0';

		errno = 0;
		char *tail_ptr;
		long code = strtol(line_buf + start, &tail_ptr, 0);

		/* FIXME: we can recover from this */
		if (errno) {
			perror("strtol()");
			exit(EXIT_FAILURE);
		}

		/* Consume whitespace */
		for (start = 0; isspace(tail_ptr[start]); start++);
		if (tail_ptr[start] == '\0')
			continue;

		/* Add the entry to the code table */
		code_table[code].fname = strdup(tail_ptr + start);
		ret = open(code_table[code].fname, OPEN_FLAGS, OPEN_MODE);
		if (ret == -1) {
			perror("open() in config init");
			exit(EXIT_FAILURE);
		}
	}

	/* Free the space created by getline */
	free(line_buf);
}
