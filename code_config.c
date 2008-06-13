#define _GNU_SOURCE

#include "data.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <ctype.h> /* isspace */
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "code_config.h"

/* This is 0 if load_config has never been called and 1 if it has */
static int ever_inited = 0;

/* This runs once when the program starts -- it's also the routine called when
 * the program receives a SIGHUP.
 *
 * FIXME: This function does extra open/close and malloc/free operations (this
 * is a pretty low hanging fruit -- the file can't have more than a few hundred
 * codes, so this function is going to be really fast anyhow). I'm think it may
 * leak a few bytes per rule.
 */
void load_code_config(int dummy) {
	int i, ret;
	uint32_t parse_error_lines[MAX_CODE];

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

#define PERR(e) (line_num | (e << 16))

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
			parse_error_lines[num_parse_errors++] = PERR(PARSE_ERR_GEN);
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
			parse_error_lines[num_parse_errors++] = PERR(PARSE_ERR_CODE);
			continue;
		}

		/* If none of line_buf was consumed, there wasn't a number on the line,
		 * and the line has a parse error */
		if (tail_ptr == line_buf) {
			parse_error_lines[num_parse_errors++] = PERR(PARSE_ERR_CODE);
			continue;
		}

		if (tail_ptr[0] == '\0') {
			parse_error_lines[num_parse_errors++] = PERR(PARSE_ERR_LOGN);
			continue;
		}

		/* Try to consume whitespace */
		for (start = 0; isspace(tail_ptr[start]); start++);
		if (!start) {
			parse_error_lines[num_parse_errors++] = PERR(PARSE_ERR_CODE);
			continue;
		}

		code_table[code] = malloc(sizeof(file_tbl));

		/* I think there's a better way to do this in C, I just don't know how */
		char *full_name = malloc(strlen(tail_ptr + start) + strlen(".splog") + 10);
		full_name[0] = '\0';
		strcat(full_name, tail_ptr + start);
		strcat(full_name, ".splog");

		if (!strcmp(full_name, SPLOGGER_UNKNOWN_LOG)) {
			parse_error_lines[num_parse_errors++] = PERR(PARSE_ERR_RESERVED);
			continue;
		}

		/* Add the entry to the code table */
		code_table[code]->fname = full_name;
		ret = open(full_name, OPEN_FLAGS, OPEN_MODE);
		if (ret == -1) {
			perror("open() in config init");
			parse_error_lines[num_parse_errors++] = PERR(PARSE_ERR_FILE);
			code_table[code] = NULL;
			continue;
		}
		code_table[code]->fd = ret;
	}

	/* Print out lines with syntax errors */
	if (num_parse_errors) {
		fprintf(stderr, "There were errors parsing/executing the following lines from %s:\n",
				config_name);
		for (i = 0; i < num_parse_errors; i++) {
			uint16_t lnum = parse_error_lines[i] & (MAX_CODE - 1);
			uint16_t err_code = parse_error_lines[i] >> 16;
			switch (err_code) {
				case PARSE_ERR_GEN:
					fprintf(stderr, "%hu: unknown/libc error\n", lnum);
					break;
				case PARSE_ERR_CODE:
					fprintf(stderr, "%hu: could not read code\n", lnum);
					break;
				case PARSE_ERR_FILE:
					fprintf(stderr, "%hu: could not open log file\n", lnum);
					break;
				case PARSE_ERR_LOGN:
					fprintf(stderr, "%hu: rule had a code but no name\n", lnum);
					break;
				case PARSE_ERR_RESERVED:
					fprintf(stderr, "%hu: a reserved name was specified\n", lnum);
					break;
				default:
					fprintf(stderr, "%hu: unexpected error, unknown error code %hu\n",
							lnum, err_code);
					break;
			}
		}
	}

	fclose(config_file);

	if (!ever_inited) {
		unknown_fd = open(SPLOGGER_UNKNOWN_LOG, OPEN_FLAGS, OPEN_MODE);
		ever_inited++;
	}

	/* Free the space created by getline */
	/*free(line_buf);*/
}
