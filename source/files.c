#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <sysutil/video.h>
#include <time.h>
#include <dirent.h>

#include "files.h"
#include "common.h"
#include "settings.h"
#include "util.h"
#include "sauce.h"
#include "ansi_types.h"
#include "list.h"

#define UTF8_CHAR_GROUP		"\xe2\x97\x86"
#define UTF8_CHAR_ITEM		"\xe2\x94\x97"

/*
 * Function:		isExist()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Whether a path exists as a file or directory
 * Arguments:
 *	path:			Path to file/dir
 * Return:			1 if true, 0 if false
 */
int isExist(const char* path)
{
	int isf = file_exists(path) == SUCCESS;
	int isd = dir_exists(path) == SUCCESS;
	
	return (isf | isd);
}

/*
 * Function:		getFileSize()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Removes the extension from a filename
 * Arguments:
 *	path:			Path to file
 * Return:			Size of file as long (== 0 if failed)
 */
long getFileSize(const char * path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return 0;
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	
	fclose(f);
	
	return fsize;
}

/*
 * Function:		endsWith()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Checks to see if a ends with b
 * Arguments:
 *	a:				String
 *	b:				Potential end
 * Return:			pointer if true, NULL if false
 */
char* endsWith(const char * a, const char * b)
{
	int al = strlen(a), bl = strlen(b);
    
	if (al < bl)
		return NULL;

	a += (al - bl);
	while (*a)
		if (*a++ != *b++) return NULL;

	return (char*) (a - bl);
}

/*
 * Function:		getDirListSize()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Calculates the number of folders in a directory
 * Arguments:
 *	path:			path to directory
 * Return:			Number of results
 */
long getDirListSize(const char * path)
{
	struct dirent *dir;
	int count = 0;
	DIR *d = opendir(path);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
				count++;
		}
		closedir(d);
	}
	
	return count;
}

int _supportedMusicFile(const char* filename)
{
	char mask[6];
	const char *music_types[] = {
		"669", "far", "it", "mod", "s3m", "ult",
		"amf", "gdm", "m15", "mtm", "stm", "uni",
		"dsm", "imf", "med", "okt", "stx", "xm",
	};

	for (int i = 0; i < 18; i++) {
		snprintf(mask, sizeof(mask), "*" ".%s", music_types[i]);
		if (wildcard_match_icase(filename, mask))
			return 1;
	}

	return 0;
}

int LoadMusicList(list_t* m_list)
{
	struct dirent *dir;
	int count = 0;

	DIR *d = opendir(ANSIVIEW_MUSIC_PATH);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if (dir->d_type == DT_REG && _supportedMusicFile(dir->d_name))
			{
				LOG("Adding Music Playlist: %s", dir->d_name);
				list_append(m_list, strdup(dir->d_name));
				count++;
			}
		}
		closedir(d);
	}
	
	return count;
}

int _getAnsiType(const char* filename)
{
	char mask[6];
	const char *ext_types[] = {
		"",
		"ans",
		"adf",
		"bin",
		"idf",
		"pcb",
		"tnd",
		"xb",
	};
	const char *ascii_types[] = {
		"asc", "nfo", "diz", "txt",
	};

	for (int i = 1; i < ANSILOVE_FILE_TYPES; i++) {
		snprintf(mask, sizeof(mask), "*" ".%s", ext_types[i]);
		if (wildcard_match_icase(filename, mask))
			return (i);
	}

	for (int i = 0; i < 4; i++) {
		snprintf(mask, sizeof(mask), "*" ".%s", ascii_types[i]);
		if (wildcard_match_icase(filename, mask))
			return ANSILOVE_FILETYPE_ANS;
	}

	return ANSILOVE_FILETYPE_UNKNOWN;
}

int _supportedAnsiFile(const char* filename)
{
	return (_getAnsiType(filename) > 0);
}

long getDirContentSize(const char * path)
{
	struct dirent *dir;
	int count = 0;
	DIR *d = opendir(path);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if (dir->d_type == DT_REG && _supportedAnsiFile(dir->d_name))
				count++;
		}
		closedir(d);
	}
	
	return count;
}

/*
 * Function:		readFile()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		reads the contents of a file into a new buffer
 * Arguments:
 *	path:			Path to file
 * Return:			Pointer to the newly allocated buffer
 */
char * readFile(const char * path, long* size)
{
	FILE *f = fopen(path, "rb");

	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fsize <= 0)
	{
		fclose(f);
		return NULL;
	}

	char * string = malloc(fsize + 1);
	fread(string, fsize, 1, f);
	fclose(f);

	*size = fsize;
	return string;
}

/*
 * Function:		rtrim()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Trims ending white spaces (' ') from a string
 * Arguments:
 *	buffer:			String
 * Return:			Amount of characters removed
 */
int rtrim(char * buffer)
{
	int i, max = strlen(buffer) - 1;
	for (i = max; (buffer[i] == ' ') && (i >= 0); i--)
		buffer[i] = 0;

	return (max - i);
}

void _setManualCode(file_entry_t* entry, uint8_t type, const char* name, const char* code)
{
	entry->type = type;
	entry->activated = 0;
//	entry->options_count = 0;
//	entry->options = NULL;
	asprintf(&entry->name, name);
//	asprintf(&entry->files, code);
}

option_entry_t* _initOptions(int count)
{
	option_entry_t* options = (option_entry_t*)malloc(sizeof(option_entry_t));

	options->id = 0;
	options->sel = -1;
	options->size = count;
	options->line = NULL;
	options->value = malloc (sizeof(char *) * count);
	options->name = malloc (sizeof(char *) * count);

	return options;
}

/*
 * Function:		ReadLocalCodes()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads an entire NCL file into an array of code_entry
 * Arguments:
 *	path:			Path to ncl
 *	_count_count:	Pointer to int (set to the number of files within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadFiles(folder_entry_t * folder)
{
    int file_count = 0, cur_count = 0;
	file_entry_t * ptr;
	char filePath[256] = "";

	file_count = getDirContentSize(folder->path);

	if (!file_count)
		return 0;

	struct dirent *dir;
	DIR *d = opendir(folder->path);

	if (!d)
		return 0;

	ptr = (file_entry_t *)calloc(1, sizeof(file_entry_t) * file_count);

	folder->file_count = file_count;
	folder->files = ptr;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type == DT_REG && _supportedAnsiFile(dir->d_name))
		{
			snprintf(filePath, sizeof(filePath), "%s%s", folder->path, dir->d_name);
			LOG("ReadLocalFolderList() :: Reading %s...", filePath);

//			ptr[cur_count].flags = 0;
			ptr[cur_count].details = NULL;
			ptr[cur_count].options = NULL;
			ptr[cur_count].options_count = 0;
			ptr[cur_count].type = _getAnsiType(dir->d_name);
			asprintf(&ptr[cur_count].file, "%s", dir->d_name);

			if (ptr[cur_count].type == ANSILOVE_FILETYPE_BIN)
			{
				ptr[cur_count].options_count = 1;
				ptr[cur_count].options = _initOptions(2);

				asprintf(&ptr[cur_count].options->name[0], "80 Columns");
				asprintf(&ptr[cur_count].options->value[0], "%c", 80);
				asprintf(&ptr[cur_count].options->name[1], "160 Columns");
				asprintf(&ptr[cur_count].options->value[1], "%c", 160);
			}

			/* let's check the file for a valid SAUCE record */
			ptr[cur_count].sauce = sauceReadFileName(filePath);

			/* if we find a SAUCE record, update bool flag */
			if (ptr[cur_count].sauce)
			{
				asprintf(&ptr[cur_count].name, "%s - %s", dir->d_name, ptr[cur_count].sauce->title);
			}
			else
			{
				LOG("File %s does not have a SAUCE record.", dir->d_name);
				asprintf(&ptr[cur_count].name, "%s", dir->d_name);
			}

			LOG("[%d] file '%s' name '%s'", ptr[cur_count].type, ptr[cur_count].file, ptr[cur_count].name);
			cur_count++;
		}
	}
		
	closedir(d);

	LOG("cur_count=%d,file_count=%d", cur_count, file_count);

	return cur_count;
}

/*
 * Function:		ReadOnlineFiles()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads an entire NCL file into an array of code_entry
 * Arguments:
 *	filename:		File name ncl
 *	_count_count:	Pointer to int (set to the number of files within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadOnlineFiles(folder_entry_t * game)
{ 
	char path[256], url[256];
	snprintf(path, sizeof(path), ANSIVIEW_LOCAL_CACHE "[ls]%s_%s.txt", game->title_id, game->name);
	snprintf(url, sizeof(url), ONLINE_URL "%s%s/raw/*", game->path, game->name);

	if (file_exists(path) == SUCCESS)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			ftp_util(url, path);
	}
	else
	{
		if (ftp_util(url, path) != SUCCESS)
			return -1;
	}

	long fsize;
	char *data = readFile(path, &fsize);
	data[fsize] = 0;
	
	char *ptr = data;
	char *end = data + fsize + 1;

	int game_count = 0;

	while (ptr < end && *ptr)
	{
		if (*ptr == '\n')
		{
			game_count++;
		}
		ptr++;
	}
	
	if (!game_count)
		return -1;

	file_entry_t * ret = (file_entry_t *)malloc(sizeof(file_entry_t) * game_count);
	game->file_count = game_count;

	int cur_count = 0;
	ptr = data;
	
	while (ptr < end && *ptr && cur_count < game_count)
	{
		char* content = ptr;

		while (ptr < end && *ptr != '\n' && *ptr != '\r')
		{
			ptr++;
		}
		*ptr++ = 0;

//        LOG("ReadLocalFolderList() :: Reading %s...", content);
		if (content[0] == 'F' && content[1] == '=')
		{
			content += 2;

			ret[cur_count].activated = 0;
			ret[cur_count].sauce = NULL;
			ret[cur_count].details = NULL;
			ret[cur_count].options = NULL;
			ret[cur_count].options_count = 0;
			ret[cur_count].type = _getAnsiType(content);

			asprintf(&ret[cur_count].name, "%s", content);
			asprintf(&ret[cur_count].file, "%s", content);

			if (ret[cur_count].type == ANSILOVE_FILETYPE_BIN)
			{
				ret[cur_count].options_count = 1;
				ret[cur_count].options = _initOptions(2);

				asprintf(&ret[cur_count].options->name[0], "80 Columns");
				asprintf(&ret[cur_count].options->value[0], "%c", 80);
				asprintf(&ret[cur_count].options->name[1], "160 Columns");
				asprintf(&ret[cur_count].options->value[1], "%c", 160);
			}

			LOG("%d - [%s] %s", cur_count, ret[cur_count].file, ret[cur_count].name);
			cur_count++;
		}

		if (ptr < end && *ptr == '\r')
		{
			ptr++;
		}
		if (ptr < end && *ptr == '\n')
		{
			ptr++;
		}
	}

	if (data) free(data);

	game->files = ret;
	return cur_count;
}

int ReadMusicFiles(folder_entry_t * folder, list_t* m_list)
{
	int count = 0;
	list_node_t* node = m_list->head;

	file_entry_t * ret = calloc(1, sizeof(file_entry_t) * m_list->count);
	folder->file_count = m_list->count;
	folder->files = ret;

	LOG("Loading music file list...");

	while (node)
	{
		ret[count].activated = 1;
		ret[count].type = ANSILOVE_FILETYPE_UNKNOWN;
		ret[count].name = list_get(node);

		LOG("[%d] Added %s", count, ret[count].name);
		node = node->next;
		count++;
	}

	LOG("%d files loaded", count);
	return count;
}

/*
 * Function:		UnloadFolderList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Free entire array of game_entry
 * Arguments:
 *	list:			Array of game_entry to free
 *	count:			number of game entries
 * Return:			void
 */
void UnloadFolderList(folder_entry_t * list, int count)
{
	if (list)
	{
		int x = 0, y = 0, z = 0;
		for (x = 0; x < count; x++)
		{
			if (list[x].name)
			{
				free(list[x].name);
				list[x].name = NULL;
			}

			if (list[x].path)
			{
				free(list[x].path);
				list[x].path = NULL;
			}

			if (list[x].title_id)
			{
				free(list[x].title_id);
				list[x].title_id = NULL;
			}
			
			if (list[x].files)
			{
				for (y = 0; y < list[x].file_count; y++)
				{
					if (list[x].files[y].sauce)
					{
						sauceFree(list[x].files[y].sauce);
//						free (list[x].files[y].sauce);
						list[x].files[y].sauce = NULL;
					}
					if (list[x].files[y].name)
					{
						free (list[x].files[y].name);
						list[x].files[y].name = NULL;
					}
					if (list[x].files[y].details)
					{
						free (list[x].files[y].details);
						list[x].files[y].details = NULL;
					}
					if (list[x].files[y].options && list[x].files[y].options_count > 0)
					{
						for (z = 0; z < list[x].files[y].options_count; z++)
						{
							if (list[x].files[y].options[z].line)
								free(list[x].files[y].options[z].line);
							if (list[x].files[y].options[z].name)
								free(list[x].files[y].options[z].name);
							if (list[x].files[y].options[z].value)
								free(list[x].files[y].options[z].value);
						}
						
						free (list[x].files[y].options);
					}
				}
				
				free(list[x].files);
				list[x].files = NULL;
			}
		}
		
		free (list);
		list = NULL;
	}
	
	LOG("UnloadFolderList() :: Successfully unloaded game list");
}

int qsortFileList_Compare(const void* itemA, const void* itemB)
{
	file_entry_t* a = (file_entry_t*) itemA;
	file_entry_t* b = (file_entry_t*) itemB;

	if (!a->name || !b->name)
		return 0;

	return strcasecmp(a->name, b->name);
}

/*
 * Function:		qsortFolderList_Compare()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Compares two game_entry for QuickSort
 * Arguments:
 *	a:				First code
 *	b:				Second code
 * Return:			1 if greater, -1 if less, or 0 if equal
 */
int qsortFolderList_Compare(const void* a, const void* b)
{
	return strcasecmp(((folder_entry_t*) a)->name, ((folder_entry_t*) b)->name);
}

/*
 * Function:		ReadLocalFolderList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads the entire userlist folder into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
folder_entry_t * ReadLocalFolderList(const char* userPath, int * gmc)
{
	int save_count = getDirListSize(userPath);
	*gmc = save_count;

	if (!save_count)
		return NULL;

	struct dirent *dir;
	DIR *d = opendir(userPath);

	if (!d)
		return NULL;

	folder_entry_t * ret = (folder_entry_t *)malloc(sizeof(folder_entry_t) * save_count);

	int cur_count = 0;

	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		if (dir->d_type == DT_DIR)
		{
			LOG("ReadLocalFolderList() :: Reading %s...", dir->d_name);

			ret[cur_count].flags = 0;
			ret[cur_count].files = NULL;
			ret[cur_count].file_count = 0;

			asprintf(&ret[cur_count].path, "%s%s/", userPath, dir->d_name);
			asprintf(&ret[cur_count].name, "%s", dir->d_name);
			asprintf(&ret[cur_count].title_id, "%ld Files", getDirContentSize(ret[cur_count].path));

			LOG("[%s] name '%s'", ret[cur_count].path, ret[cur_count].name);

			cur_count++;
		}
	}
		
	closedir(d);
	
	return ret;
}

/*
 * Function:		ReadOnlineList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads the entire gamelist file into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
folder_entry_t * ReadOnlineList(const char* urlPath, int * gmc)
{
	char ftpurl[256], path[256];
	const char* folder = urlPath + strlen(ONLINE_URL);

	snprintf(ftpurl, sizeof(ftpurl), "%s" "*", urlPath);
	snprintf(path, sizeof(path), ANSIVIEW_LOCAL_CACHE "[ls]%s" "txt", folder);
	*strrchr(path, '/') = '.';

	if (file_exists(path) == SUCCESS)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			ftp_util(ftpurl, path);
	}
	else
	{
		if (ftp_util(ftpurl, path) != SUCCESS)
			return NULL;
	}

	long fsize;
	char *data = readFile(path, &fsize);
	data[fsize] = 0;
	
	char *ptr = data;
	char *end = data + fsize + 1;

	int game_count = 0;

	while (ptr < end && *ptr)
	{
		if (*ptr == '\n')
		{
			game_count++;
		}
		ptr++;
	}
	
	if (!game_count)
		return NULL;

	folder_entry_t * ret = (folder_entry_t *)malloc(sizeof(folder_entry_t) * game_count);
	*gmc = game_count;

	int cur_count = 0;    
	ptr = data;
	
	while (ptr < end && *ptr && cur_count < game_count)
	{
		char* content = ptr;

		while (ptr < end && *ptr != '\n' && *ptr != '\r')
		{
			ptr++;
		}
		*ptr++ = 0;

//        LOG("ReadLocalFolderList() :: Reading %s...", content);
		if (content[0] == 'D' && content[1] == '=')
		{
			content += 2;

			ret[cur_count].path = strdup(folder);
			ret[cur_count].files = NULL;
			ret[cur_count].flags = FOLDER_FLAG_FTP;
			ret[cur_count].file_count = 0;

			ret[cur_count].title_id = strdup(folder);
			ret[cur_count].title_id[4] = 0;

			asprintf(&ret[cur_count].name, "%s", content);

			LOG("%d - [%s] %s", cur_count, ret[cur_count].title_id, ret[cur_count].name);
			cur_count++;
		}

		if (ptr < end && *ptr == '\r')
		{
			ptr++;
		}
		if (ptr < end && *ptr == '\n')
		{
			ptr++;
		}
	}

	if (data) free(data);

	return ret;
}

/*
 * Function:		isGameActivated()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Checks if the game has been selected/activated by the user
 * Arguments:
 *	game:			Game to read
 * Return:			1 if selected, 0 if not selected
 */
int isGameActivated(folder_entry_t game)
{
	int x = 0;
	for (x = 0; x < game.file_count; x++)
	{
		if (game.files)
		{
			if (game.files[x].activated)
				return 1;
		}
	}
	
	return 0;
}
