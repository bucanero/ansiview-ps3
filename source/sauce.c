/*
 * sauce.c
 * Ansilove 4.1.0
 * https://www.ansilove.org
 *
 * Copyright (c) 2011-2020 Stefan Vogt, Brian Cassidy, and Frederic Cambus
 * All rights reserved.
 *
 * Ansilove is licensed under the BSD 2-Clause License.
 * See LICENSE file for details.
 */

#define _XOPEN_SOURCE 700

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sauce.h"

/* Reads SAUCE via a filename. */
struct sauce *
sauceReadFileName(char *fileName)
{
	FILE *file = fopen(fileName, "r");
	if (file == NULL) {
		return NULL;
	}

	struct sauce *record = sauceReadFile(file);
	fclose(file);
	return record;
}

/* Read SAUCE via a FILE pointer. */
struct sauce *
sauceReadFile(FILE *file)
{
	struct sauce *record;
	record = malloc(sizeof *record);
	memset(record, 0, sizeof *record);

	if (record != NULL) {
		readRecord(file, record);

		if (strcmp(record->ID, SAUCE_ID) != 0) {
			free(record);
			record = NULL;
		}
	}
	return record;
}

void
readRecord(FILE *file, struct sauce *record)
{
	if (fseek(file, 0 - RECORD_SIZE, SEEK_END) != 0) {
		return;
	}

	size_t read_status = fread(record->ID, sizeof (record->ID) - 1, 1, file);
	record->ID[sizeof (record->ID) - 1] = '\0';

	if (read_status != 1 || strcmp(record->ID, SAUCE_ID) != 0) {
		return;
	}
	fread(record->version, sizeof (record->version) - 1, 1, file);
	record->version[sizeof (record->version) - 1] = '\0';
	fread(record->title, sizeof (record->title) - 1, 1, file);
	record->title[sizeof (record->title) - 1] = '\0';
	fread(record->author, sizeof (record->author) - 1, 1, file);
	record->author[sizeof (record->author) - 1] = '\0';
	fread(record->group, sizeof (record->group) - 1, 1, file);
	record->group[sizeof (record->group) - 1] = '\0';
	fread(record->date, sizeof (record->date) - 1, 1, file);
	record->date[sizeof (record->date) - 1] = '\0';
	fread(&(record->fileSize), sizeof (record->fileSize), 1, file);
	fread(&(record->dataType), sizeof (record->dataType), 1, file);
	fread(&(record->fileType), sizeof (record->fileType), 1, file);
	fread(&(record->tinfo1), sizeof (record->tinfo1), 1, file);
	fread(&(record->tinfo2), sizeof (record->tinfo2), 1, file);
	fread(&(record->tinfo3), sizeof (record->tinfo3), 1, file);
	fread(&(record->tinfo4), sizeof (record->tinfo4), 1, file);
	fread(&(record->comments), sizeof (record->comments), 1, file);
	fread(&(record->flags), sizeof (record->flags), 1, file);
	fread(record->tinfos, sizeof (record->tinfos) - 1, 1, file);
	record->tinfos[sizeof (record->tinfos) - 1] = '\0';

	if (ferror(file) != 0) {
		return;
	}

	if (record->comments > 0) {
		record->comment_lines = malloc(record->comments *sizeof (*record->comment_lines));

		if (record->comment_lines != NULL) {
			if (readComments(file, record->comment_lines, record->comments) == -1) {
				record->comments = 0;
			}
		}
	}
}

int
readComments(FILE *file, char **comment_lines, int32_t comments)
{
	int32_t i;

	if (fseek(file, 0 - (RECORD_SIZE + 5 + COMMENT_SIZE *comments), SEEK_END) == 0) {
		char ID[6];
		fread(ID, sizeof (ID) - 1, 1, file);
		ID[sizeof (ID) - 1] = '\0';

		if (strcmp(ID, COMMENT_ID) != 0) {
			return -1;
		}

		for (i = 0; i < comments; i++) {
			char buf[COMMENT_SIZE + 1] = "";

			fread(buf, COMMENT_SIZE, 1, file);
			buf[COMMENT_SIZE] = '\0';

			if (ferror(file) == 0) {
				comment_lines[i] = strdup(buf);
				if (comment_lines[i] == NULL) {
					return -1;
				}
			} else {
				return -1;
			}
		}
		return 0;
	}
	return -1;
}

void sauceFree(struct sauce *record)
{
	if (record->comments > 0) {
		for (int i = 0; i < record->comments; i++)
			free(record->comment_lines[i]);

		free(record->comment_lines);
	}
	free(record);
};

char* sauceStringDataType(uint8_t dt)
{
	switch (dt)
	{
	case 0:
		return "None";
	
	case 1:
		return "Character";

	case 2:
		return "Bitmap";

	case 3:
		return "Vector";

	case 4:
		return "Audio";

	case 5:
		return "BinaryText";

	case 6:
		return "XBin";

	case 7:
		return "Archive";

	case 8:
		return "Executable";

	default:
		return "Undefined";
	}
}
