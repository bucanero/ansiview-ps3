#ifdef DEBUG_ENABLE_LOGGING
#include <dbglogger.h>
#define LOG dbglogger_log
#else
#define LOG(...)
#define dbglogger_init(...)
#endif

#include "sauce.h"
#include "list.h"

#define ANSIVIEW_PATH			"/dev_hdd0/game/NP0ANSIVW/USRDIR/"
#define ANSIVIEW_DATA_PATH		ANSIVIEW_PATH "DATA/"
#define ANSIVIEW_MUSIC_PATH		ANSIVIEW_PATH "MUSIC/"
#define ANSIVIEW_LOCAL_CACHE	ANSIVIEW_PATH "CACHE/"
#define ANSIVIEW_UPDATE_URL		"https://api.github.com/repos/bucanero/ansiview-ps3/releases/latest"

#define USB0_PATH               "/dev_usb000/"
#define USB1_PATH               "/dev_usb001/"
#define ANSIVIEW_PATH_USB0		USB0_PATH "ANSIVIEW/"
#define ANSIVIEW_PATH_USB1		USB1_PATH "ANSIVIEW/"

#define EXPORT_PATH_USB0        USB0_PATH "EXPORT/"
#define EXPORT_PATH_USB1        USB1_PATH "EXPORT/"

#define ONLINE_URL				"ftp://16colo.rs/pack/"
#define ONLINE_CACHE_TIMEOUT    24*3600     // 1-day local cache

// Save flags
#define SAVE_FLAG_LOCKED        1
#define FOLDER_FLAG_FTP         2
#define FILE_FLAG_SAUCE         4
#define SAVE_FLAG_PS3           8
#define FOLDER_FLAG_YEAR        16
#define FILE_FLAG_PSP           32
#define FILE_FLAG_PSV           64

enum char_flag_enum
{
    CHAR_TAG_NULL,
    CHAR_TAG_PS1,
    CHAR_TAG_PS2,
    CHAR_TAG_PS3,
    CHAR_TAG_PSP,
    CHAR_TAG_PSV,
    CHAR_TAG_APPLY,
    CHAR_TAG_OWNER,
    CHAR_TAG_LOCKED,
    CHAR_RES_TAB,
    CHAR_RES_LF,
    CHAR_TAG_TRANSFER,
    CHAR_TAG_ZIP,
    CHAR_RES_CR,
    CHAR_TAG_PCE,
    CHAR_TAG_WARNING,
    CHAR_BTN_X,
    CHAR_BTN_S,
    CHAR_BTN_T,
    CHAR_BTN_O,
};

typedef struct option_entry
{
    char * line;
    char * * name;
    char * * value;
    int id;
    int size;
    int sel;
} option_entry_t;

typedef struct file_entry
{
    unsigned char type;
    char * name;
    char * file;
    unsigned char activated;
//	unsigned int flags;
    sauce_record_t * sauce;
    char * details;
    int options_count;
    option_entry_t * options;
} file_entry_t;

typedef struct folder_entry
{
    char * name;
	char * title_id;
	char * path;
	unsigned int flags;
    int file_count;
    file_entry_t * files;
} folder_entry_t;

typedef struct
{
    folder_entry_t * folders;
    int count;
    char path[128];
    char* title;
    unsigned char icon_id;
    void (*UpdatePath)(char *);
    int (*ReadFiles)(folder_entry_t *);
    folder_entry_t* (*ReadFolders)(const char*, int *);
} menu_list_t;

folder_entry_t * ReadLocalFolderList(const char* userPath, int * gmc);
folder_entry_t * ReadOnlineList(const char* urlPath, int * gmc);
void UnloadFolderList(folder_entry_t * list, int count);
int isGameActivated(folder_entry_t game);
char * ParseActivatedGameList(folder_entry_t * list, int count);
char * readFile(const char * path, long* size);
int qsortFolderList_Compare(const void* A, const void* B);
int qsortFileList_Compare(const void* A, const void* B);
long getFileSize(const char * path);
int ReadFiles(folder_entry_t * folder);
int ReadOnlineFiles(folder_entry_t * game);
int LoadMusicList(list_t* m_list);
int ReadMusicFiles(folder_entry_t * folder, list_t* m_list);

int http_init(void);
void http_end(void);
int http_download(const char* url, const char* filename, const char* local_dst, int show_progress);
int ftp_util(const char *ftp_url, const char* dst_name);

int extract_zip(const char* zip_file, const char* dest_path);
int zip_directory(const char* basedir, const char* inputdir, const char* output_zipfile);

int show_dialog(int dialog_type, const char * str);
void init_progress_bar(const char* progress_bar_title, const char* msg);
void update_progress_bar(long unsigned int* progress, const long unsigned int total_size, const char* msg);
void end_progress_bar(void);

int init_loading_screen(const char* msg);
void stop_loading_screen();

int apply_cheat_patch_code(const char* fpath, const char* title_id, file_entry_t* code);
