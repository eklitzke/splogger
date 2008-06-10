#define _GNU_SOURCE /* Use GNU extensions, like GNU getline */

#include <sp.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <ctype.h> /* isspace */
#include <unistd.h>

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
static file_tbl* code_table[MAX_CODE];

void shutdown_cleanly(int dummy);
void load_config(int dummy);

int main(int argc, char **argv) {
	int c, i;
	int ret;
	int add_newlines = 0;
	int daemon = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "hng:c:d")) != -1) {
		switch (c) {
			case 'h':
				printf("Usage: splogger -g group_name [-h] [-c config_name] [-d]\n");
				exit(EXIT_SUCCESS);
				break;
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
	load_config(0);

	char pgroupname[MAX_GROUP_NAME];

	signal(SIGHUP, load_config);
	signal(SIGINT, shutdown_cleanly);
	signal(SIGQUIT, shutdown_cleanly);

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

		/* Handle error conditions */
		if (bytes_recvd < 0) {
			printf("br %d\n", bytes_recvd);
			splogger_fail(bytes_recvd);
		}

		if (mess_type >= MAX_CODE) {
			fprintf(stderr, "Got unexpected code %d.\n", mess_type);
			exit(EXIT_FAILURE);
		}

		file_tbl *ent = code_table[mess_type];
		if (ent == NULL) {
			fprintf(stderr, "Code %d does not exit in code table.\n", mess_type);
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

	shutdown_cleanly(0);
}

void shutdown_cleanly(int dummy) {
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

	exit(EXIT_SUCCESS);
}

/* This runs once when the program starts -- it's also the routine called when
 * the program receives a SIGHUP.
 *
 * TODO: Better parse error handling (indicate what kind of error).
 *
 * FIXME: This function does extra open/close and malloc/free operations
 */
void load_config(int dummy) {
	int i, ret;
	int parse_error_lines[MAX_CODE];

	/* Clear out the old table, clear the list of line errors */
	for (i = 0; i < MAX_CODE; i++) {
		file_tbl *ent = code_table[i];
		if (ent == NULL)
			continue;
		close(ent->fd);
		free(ent->fname);

		parse_error_lines[i] = 0;
	}

	int start, end;

	FILE *config_file = fopen(config_name, "r");
	if (config_file == NULL) {
		perror("fopen()");
		exit(EXIT_FAILURE);
	}

	int line_num = 0;
	int num_parse_errors = 0;

	size_t buf_size = 0;
	char *line_buf = NULL;
	while (1) {
		line_num++;
		start = 0;
		errno = 0;
		ret = getline(&line_buf, &buf_size, config_file);
		if (ret == -1) {
			/* Break out of the loop on EOF */
			if (errno == 0)
				break;
			perror("getline()");
			parse_error_lines[num_parse_errors++] = line_num;
			continue;
		}

		/* Strip initial whitespace */
		for (start = 0; isspace(line_buf[start]); start++);
		if (line_buf[start] == '\0')
			continue;

		/* Strip the comment if present */
		for (end = start; (line_buf[end] != '#') && (line_buf[end] != '\0'); end++);

		/* Strip trailing whitespace */
		for (end--; (end >= start) && isspace(line_buf[end]); end--);
		if (end < start)
			continue;

		/* Null terminate the string */
		line_buf[end + 1] = '\0';

		errno = 0;
		char *tail_ptr;
		long code = strtol(line_buf + start, &tail_ptr, 0);

		if (errno) {
			perror("strtol()");
			parse_error_lines[num_parse_errors++] = line_num;
			continue;
		}

		/* If none of line_buf was consumed, there wasn't a number on the line,
		 * and the line has a parse error */
		if (tail_ptr == line_buf) {
			parse_error_lines[num_parse_errors++] = line_num;
			continue;
		}

		/* Consume whitespace */
		for (start = 0; isspace(tail_ptr[start]); start++);
		if (!start || tail_ptr[start] == '\0') {
			parse_error_lines[num_parse_errors++] = line_num;
			continue;
		}

		code_table[code] = malloc(sizeof(file_tbl));

		/* I think there's a better way to do this in C, I just don't know how */
		char *full_name = malloc(strlen(tail_ptr + start) + strlen(".splog") + 10);
		full_name[0] = '\0';
		strcat(full_name, tail_ptr + start);
		strcat(full_name, ".splog");

		/* Add the entry to the code table */
		code_table[code]->fname = full_name;
		ret = open(full_name, OPEN_FLAGS, OPEN_MODE);
		if (ret == -1) {
			perror("open() in config init");
			parse_error_lines[num_parse_errors++] = line_num;
			code_table[code] = NULL;
			continue;
		}
		code_table[code]->fd = ret;
	}

	/* Print out lines with syntax errors */
	if (num_parse_errors) {
		fprintf(stderr, "The following lines had parse errors (or the log file could not be opened):");
		for (i = 0; i < num_parse_errors; i++)
			fprintf(stderr, " %d", parse_error_lines[i]);
		fputc('\n', stderr);
	}

	fclose(config_file);

	/* Free the space created by getline */
	/*free(line_buf);*/
}

/* vim: set tw=4 sw=4 noet */
