/* 
	ANSi View PS3 main.c
*/

#include <ppu-lv2.h>
#include <sys/file.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <sysutil/video.h>
#include <lv2/process.h>
#include <time.h>
#include <dirent.h>

#include <errno.h>

#include <zlib.h>
#include <io/pad.h>

#include <sysmodule/sysmodule.h>
#include <sysutil/sysutil.h>
#include <sys/thread.h>

#include <tiny3d.h>
#include <ansilove.h>
#include <mikmod.h>
#include "files.h"
#include "util.h"
#include "ansi_types.h"
#include "common.h"
#include "list.h"

//Menus
#include "menu.h"
#include "menu_about.h"
#include "menu_options.h"
#include "menu_screens.h"

enum menu_screen_ids
{
	MENU_MAIN_SCREEN,
	MENU_BROWSE_USB,
	MENU_BROWSE_HDD,
	MENU_ONLINE_DB,
	MENU_MUSIC_LIST,
	MENU_SETTINGS,
	MENU_CREDITS,
	MENU_ANSFILES,
	MENU_DETAILS_VIEW,
	MENU_FILE_OPTIONS,
	MENU_ONLINE_LIST,
	TOTAL_MENU_IDS
};

//Font
#include "libfont.h"
#include "ttf_render.h"
#include "font-16x32.h"


// SPU
u32 inited;
//u32 spu = 0;
//sysSpuImage spu_image;

#define INITED_CALLBACK     1
#define INITED_SPU          2
#define INITED_SOUNDLIB     4
#define INITED_AUDIOPLAYER  8

#define SPU_SIZE(x) (((x)+127) & ~127)

#define load_menu_texture(name, type) \
	({ extern const uint8_t name##_##type []; \
	   extern const uint32_t name##_##type##_size; \
	   menu_textures[name##_##type##_index].buffer = (const void*) name##_##type; \
	   menu_textures[name##_##type##_index].size = name##_##type##_size; \
	   LoadTexture(name##_##type##_index); \
	})

#define show_message(msg)	show_dialog(0, msg)

//Pad stuff
padInfo padinfo;
padData paddata[MAX_PADS];
padData padA[MAX_PADS];
padData padB[MAX_PADS];

void music_callback(int sel);
void sort_callback(int sel);
void ani_callback(int sel);
void horm_callback(int sel);
void verm_callback(int sel);
void update_callback(int sel);
void clearcache_callback(int sel);
void upd_appdata_callback(int sel);

void update_usb_path(char *p);
void update_hdd_path(char *p);

app_config_t app_config = {
    .music = 1,
    .doSort = 1,
    .doAni = 1,
    .update = 1,
    .marginH = 0,
    .marginV = 0,
    .exportPNG = 0,
    .autoScroll = 1,
};

const menu_option_t menu_options[] = {
	{ .name = "Background Music", .options = NULL, .type = APP_OPTION_BOOL, .value = &app_config.music, .callback = music_callback },
	{ .name = "Sort Saves", .options = NULL, .type = APP_OPTION_BOOL, .value = &app_config.doSort, .callback = sort_callback },
	{ .name = "Menu Animations", .options = NULL, .type = APP_OPTION_BOOL, .value = &app_config.doAni, .callback = ani_callback },
	{ .name = "Screen Horizontal Margin", .options = NULL, .type = APP_OPTION_INC, .value = &app_config.marginH, .callback = horm_callback },
	{ .name = "Screen Vertical Margin", .options = NULL, .type = APP_OPTION_INC, .value = &app_config.marginV, .callback = verm_callback },
	{ .name = "Version Update Check", .options = NULL, .type = APP_OPTION_BOOL, .value = &app_config.update, .callback = update_callback },
	{ .name = "Clear Local Cache", .options = NULL, .type = APP_OPTION_CALL, .value = NULL, .callback = clearcache_callback },
//	{ .name = "Update Application Data", .options = NULL, .type = APP_OPTION_CALL, .value = NULL, .callback = upd_appdata_callback },
	{ .name = NULL }
};

int menu_options_maxopt = 0;
int * menu_options_maxsel;

int close_app = 0;
int idle_time = 0;                          // Set by readPad

png_texture * menu_textures;                // png_texture array for main menu, initialized in LoadTexture

MODULE *module;
list_t* music_files;

int (*ansi_loaders[])(struct ansilove_ctx *, struct ansilove_options *) = {
	NULL,
	ansilove_ansi,
	ansilove_artworx,
	ansilove_binary,
	ansilove_icedraw,
	ansilove_pcboard,
	ansilove_tundra,
	ansilove_xbin
};

const char * menu_about_strings[] = { "Bucanero", "Developer",
									"Dnawrkshp", "Artemis code",
									"ByteProject et al.", "libansilove",
									NULL, NULL };

/*
* 0 - Main Menu
* 1 - USB Menu (User List)
* 2 - HDD Menu (User List)
* 3 - Online Menu (Online List)
* 4 - Options Menu
* 5 - About Menu
* 6 - Code Menu (Select Cheats)
* 7 - Code Menu (View Cheat)
* 8 - Code Menu (View Cheat Options)
*/
int menu_id = 0;												// Menu currently in
int menu_sel = 0;												// Index of selected item (use varies per menu)
int menu_old_sel[TOTAL_MENU_IDS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };		// Previous menu_sel for each menu
int last_menu_id[TOTAL_MENU_IDS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };		// Last menu id called (for returning)

const char * menu_pad_help[TOTAL_MENU_IDS] = { NULL,												//Main
								"\x10 Select    \x13 Back    \x11 Refresh",							//USB list
								"\x10 Select    \x13 Back    \x11 Refresh",							//HDD list
								"\x10 Select    \x13 Back    \x11 Refresh",							//Online list
								"\x10 Select    \x13 Back",											//Music list
								"\x10 Select    \x13 Back",											//Options
								"\x13 Back",														//About
								"\x10 Select    \x12 Details    \x13 Back",							//Select Files
								"\x13 Back",														//View Cheat
								"\x10 Select    \x13 Back",											//Cheat Option
								"\x13 Back",														//View Details
								};

/*
* HDD folder list
*/
menu_list_t hdd_folders = {
	.icon_id = cat_hdd_png_index,
	.title = "Browse HDD",
    .folders = NULL,
    .count = 0,
    .path = "",
    .ReadFolders = &ReadLocalFolderList,
    .ReadFiles = &ReadFiles,
    .UpdatePath = &update_hdd_path,
};

/*
* USB folder list
*/
menu_list_t usb_folders = {
	.icon_id = cat_usb_png_index,
	.title = "Browse USB",
    .folders = NULL,
    .count = 0,
    .path = "",
    .ReadFolders = &ReadLocalFolderList,
    .ReadFiles = &ReadFiles,
    .UpdatePath = &update_usb_path,
};

/*
* 16colo.rs Online folder list
*/
menu_list_t* online_folders;

/*
* Music play list
*/
folder_entry_t music_folder = {
	.name = "Music",
	.files = NULL,
};

int total_years = (2020 - 1989);
menu_list_t* selected_online_year = NULL;
folder_entry_t* selected_folder = NULL;
file_entry_t* selected_file = NULL;
int option_index = 0;

void release_all() {
	
	if(inited & INITED_CALLBACK)
		sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);

	if(inited & INITED_AUDIOPLAYER)
{}//		StopAudio();
	
	if(inited & INITED_SOUNDLIB)
{}//		SND_End();

	if(inited & INITED_SPU) {
//		sysSpuRawDestroy(spu);
//		sysSpuImageClose(&spu_image);
	}

	inited=0;
}

static void sys_callback(uint64_t status, uint64_t param, void* userdata) {

	 switch (status) {
		case SYSUTIL_EXIT_GAME: //0x0101
				
			release_all();
			sysProcessExit(1);
			break;
	  
		case SYSUTIL_MENU_OPEN: //0x0131

			break;
		case SYSUTIL_MENU_CLOSE: //0x0132

			break;
	   default:
		   break;
		 
	}
}

int pad_time = 0, rest_time = 0, pad_held_time = 0, rest_held_time = 0;
u16 oldPad = 0;

int readPad(int port)
{
	ioPadGetInfo(&padinfo);
	int off = 2;
	int retDPAD = 0, retREST = 0;
	u8 dpad = 0, _dpad = 0;
	u16 rest = 0, _rest = 0;
	
	if(padinfo.status[port])
	{
		ioPadGetData(port, &padA[port]);
		
		//new
		dpad = ((char)*(&padA[port].zeroes + off) << 8) >> 12;
		rest = ((((char)*(&padA[port].zeroes + off) & 0xF) << 8) | ((char)*(&padA[port].zeroes + off + 1) << 0));
		
		//old
		_dpad = ((char)*(&padB[port].zeroes + off) << 8) >> 12;
		_rest = ((((char)*(&padB[port].zeroes + off) & 0xF) << 8) | ((char)*(&padB[port].zeroes + off + 1) << 0));
		
		if (dpad == 0 && rest == 0)
			idle_time++;
		else
			idle_time = 0;
		
		//Copy new to old
		//memcpy(&_paddata[port].zeroes + off, &paddata[port].zeroes + off, size);
		memcpy(paddata, padA, sizeof(padData));
		memcpy(padB, padA, sizeof(padData));
		
		//DPad has 3 mode input delay
		if (dpad == _dpad && dpad != 0)
		{
			if (pad_time > 0) //dpad delay
			{
				pad_held_time++;
				pad_time--;
				retDPAD = 0;
			}
			else
			{
				//So the pad speeds up after a certain amount of time being held
				if (pad_held_time > 180)
				{
					pad_time = 2;
				}
				else if (pad_held_time > 60)
				{
					pad_time = 5;
				}
				else
					pad_time = 20;
				
				retDPAD = 1;
			}
		}
		else
		{
			pad_held_time = 0;
			pad_time = 0;
		}
		
		//rest has its own delay
		if (rest == _rest && rest != 0)
		{
			if (rest_time > 0) //rest delay
			{
				rest_held_time++;
				rest_time--;
				retREST = 0;
			}
			else
			{
				//So the pad speeds up after a certain amount of time being held
				if (rest_held_time > 180)
				{
					rest_time = 2;
				}
				else if (rest_held_time > 60)
				{
					rest_time = 5;
				}
				else
					rest_time = 20;
				
				retREST = 1;
			}
		}
		else
		{
			rest_held_time = 0;
			rest_time = 0;
		}
		
	}
	
	if (!retDPAD && !retREST)
		return 0;
	
	if (!retDPAD)
	{
		paddata[port].BTN_LEFT = 0;
		paddata[port].BTN_RIGHT = 0;
		paddata[port].BTN_UP = 0;
		paddata[port].BTN_DOWN = 0;
	}
	
	if (!retREST)
	{
		paddata[port].BTN_L2 = 0;
		paddata[port].BTN_R2 = 0;
		paddata[port].BTN_L1 = 0;
		paddata[port].BTN_R1 = 0;
		paddata[port].BTN_TRIANGLE = 0; 
		paddata[port].BTN_CIRCLE = 0;
		paddata[port].BTN_CROSS = 0;
		paddata[port].BTN_SQUARE = 0;
		paddata[port].BTN_SELECT = 0;
		paddata[port].BTN_L3 = 0;
		paddata[port].BTN_R3 = 0;
		paddata[port].BTN_START = 0;
	}
	
	return 1;
}


void LoadTexture(int cnt)
{
	pngLoadFromBuffer(menu_textures[cnt].buffer, menu_textures[cnt].size, &menu_textures[cnt].texture);

	// copy texture datas from PNG to the RSX memory allocated for textures
	if (menu_textures[cnt].texture.bmp_out)
	{
		memcpy(free_mem, menu_textures[cnt].texture.bmp_out, menu_textures[cnt].texture.pitch * menu_textures[cnt].texture.height);
		free(menu_textures[cnt].texture.bmp_out); // free the PNG because i don't need this datas
		menu_textures[cnt].texture_off = tiny3d_TextureOffset(free_mem);      // get the offset (RSX use offset instead address)
		free_mem += ((menu_textures[cnt].texture.pitch * menu_textures[cnt].texture.height + 15) & ~15) / 4; // aligned to 16 bytes (it is u32) and update the pointer
	}
}

// Used only in initialization. Allocates 64 mb for textures and loads the font
void LoadTextures_Menu()
{
	texture_mem = tiny3d_AllocTexture(64*1024*1024); // alloc 64MB of space for textures (this pointer can be global)
	
	if(!texture_mem) return; // fail!
	
	ResetFont();
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) console_font_16x32, (u8 *) texture_mem, 0, 255, 16, 32, 1, BIT7_FIRST_PIXEL);
	
	TTFUnloadFont();
	TTFLoadFont(0, "/dev_flash/data/font/SCE-PS3-SR-R-LATIN2.TTF", NULL, 0);
	TTFLoadFont(1, "/dev_flash/data/font/SCE-PS3-DH-R-CGB.TTF", NULL, 0);
	TTFLoadFont(2, "/dev_flash/data/font/SCE-PS3-SR-R-JPN.TTF", NULL, 0);
	TTFLoadFont(3, "/dev_flash/data/font/SCE-PS3-YG-R-KOR.TTF", NULL, 0);

	free_mem = (u32*) init_ttf_table((u16*) free_mem);
	
	set_ttf_window(0, 0, 848 + app_config.marginH, 512 + app_config.marginV, WIN_SKIP_LF);
//	TTFUnloadFont();
	
	if (!menu_textures)
		menu_textures = (png_texture *)malloc(sizeof(png_texture) * TOTAL_MENU_TEXTURES);
	
	//Init Main Menu textures
	load_menu_texture(bgimg, png);
	load_menu_texture(cheat, png);

	load_menu_texture(circle_loading_bg, png);
	load_menu_texture(circle_loading_seek, png);
	load_menu_texture(edit_shadow, png);

	load_menu_texture(footer_ico_circle, png);
	load_menu_texture(footer_ico_cross, png);
	load_menu_texture(footer_ico_lt, png);
	load_menu_texture(footer_ico_rt, png);
	load_menu_texture(footer_ico_square, png);
	load_menu_texture(footer_ico_triangle, png);
	load_menu_texture(header_dot, png);
	load_menu_texture(header_line, png);

	load_menu_texture(mark_arrow, png);
	load_menu_texture(mark_line, png);
	load_menu_texture(opt_off, png);
	load_menu_texture(opt_on, png);
	load_menu_texture(scroll_bg, png);
	load_menu_texture(scroll_lock, png);
	load_menu_texture(help, png);
	load_menu_texture(buk_scr, png);
	load_menu_texture(cat_about, png);
	load_menu_texture(cat_cheats, png);
	load_menu_texture(cat_opt, png);
	load_menu_texture(cat_usb, png);
	load_menu_texture(cat_bup, png);
	load_menu_texture(cat_db, png);
	load_menu_texture(cat_hdd, png);
	load_menu_texture(cat_sav, png);
	load_menu_texture(cat_warning, png);
	load_menu_texture(column_1, png);
	load_menu_texture(column_2, png);
	load_menu_texture(column_3, png);
	load_menu_texture(column_4, png);
	load_menu_texture(column_5, png);
	load_menu_texture(column_6, png);
	load_menu_texture(column_7, png);
	load_menu_texture(menu_opt_hover, png);
	load_menu_texture(logo, png);
/*
	load_menu_texture(logo_text, png);
	load_menu_texture(tag_lock, png);
	load_menu_texture(tag_own, png);
	load_menu_texture(tag_pce, png);
	load_menu_texture(tag_ps1, png);
	load_menu_texture(tag_ps2, png);
	load_menu_texture(tag_ps3, png);
	load_menu_texture(tag_psp, png);
	load_menu_texture(tag_psv, png);
	load_menu_texture(tag_warning, png);
	load_menu_texture(tag_zip, png);
	load_menu_texture(tag_apply, png);
	load_menu_texture(tag_transfer, png);
*/
	LOG("LoadTextures_Menu() :: Allocated %db (%.02fkb, %.02fmb) for textures", (free_mem - texture_mem), (free_mem - texture_mem) / (float)1024, (free_mem - texture_mem) / (float)(1024 * 1024));
}

void music_callback(int sel)
{
	app_config.music = !sel;

	if ((app_config.music && Player_Paused()) || (!app_config.music && !Player_Paused()))
		Player_TogglePause();
}

void sort_callback(int sel)
{
	app_config.doSort = !sel;
}

void ani_callback(int sel)
{
	app_config.doAni = !sel;
}

void horm_callback(int sel)
{
	if (sel == 255)
		sel = 0;
	if (sel > 100)
		sel = 100;
	app_config.marginH = sel;
}

void verm_callback(int sel)
{
	if (sel == 255)
		sel = 0;
	if (sel > 100)
		sel = 100;
	app_config.marginV = sel;
}

void clearcache_callback(int sel)
{
	DIR *d;
	struct dirent *dir;
	char dataPath[256];

	d = opendir(ANSIVIEW_LOCAL_CACHE);
	if (!d)
		return;

	LOG("Cleaning folder '%s'...", ANSIVIEW_LOCAL_CACHE);

	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
		{
			snprintf(dataPath, sizeof(dataPath), "%s" "%s", ANSIVIEW_LOCAL_CACHE, dir->d_name);
			LOG("Removing %s", dataPath);
			unlink_secure(dataPath);
		}
	}
	closedir(d);

	show_message("Local cache folder cleaned");
}

void _unzip_app_data()
{
	if (extract_zip(ANSIVIEW_LOCAL_CACHE "appdata.zip", ANSIVIEW_DATA_PATH))
		show_message("Successfully installed local application data");

	unlink_secure(ANSIVIEW_LOCAL_CACHE "appdata.zip");
}

void upd_appdata_callback(int sel)
{
	if (http_download(ONLINE_URL, "appdata.zip", ANSIVIEW_LOCAL_CACHE "appdata.zip", 1))
		_unzip_app_data();
}

void update_callback(int sel)
{
    app_config.update = !sel;

    if (!app_config.update)
        return;

	LOG("checking latest ANSi View version at %s", ANSIVIEW_UPDATE_URL);

	if (!http_download(ANSIVIEW_UPDATE_URL, "", ANSIVIEW_LOCAL_CACHE "ver.check", 0))
	{
		LOG("http request to %s failed", ANSIVIEW_UPDATE_URL);
		return;
	}

	char *buffer;
	long size = 0;

	buffer = readFile(ANSIVIEW_LOCAL_CACHE "ver.check", &size);

	if (!buffer)
		return;

	LOG("received %u bytes", size);
	buffer[size-1] = 0;

	static const char find[] = "\"name\":\"ANSi View v";
	const char* start = strstr(buffer, find);
	if (!start)
	{
		LOG("no name found");
		return;
	}

	LOG("found name");
	start += sizeof(find) - 1;

	char* end = strstr(start, "\"");
	if (!end)
	{
		LOG("no end of name found");
		return;
	}
	*end = 0;
	LOG("latest version is %s", start);

	if (stricmp(ANSIVIEW_VERSION, start) == 0)
	{
		return;
	}

	start = strstr(end+1, "\"browser_download_url\":\"");
	if (!start)
		return;

	start += 24;
	end = strstr(start, "\"");
	if (!end)
	{
		LOG("no download URL found");
		return;
	}

	*end = 0;
	LOG("download URL is %s", start);

	if (show_dialog(1, "New version available! Download update?"))
	{
		if (http_download(start, "", "/dev_hdd0/packages/ansiview-ps3.pkg", 1))
			show_message("Update downloaded!");
		else
			show_message("Download error!");
	}

}

void update_usb_path(char* path)
{
	if (dir_exists(ANSIVIEW_PATH_USB0) == SUCCESS)
		strcpy(path, ANSIVIEW_PATH_USB0);
	else if (dir_exists(ANSIVIEW_PATH_USB1) == SUCCESS)
		strcpy(path, ANSIVIEW_PATH_USB1);
	else
		strcpy(path, "");
}

void update_hdd_path(char* path)
{
	sprintf(path, ANSIVIEW_DATA_PATH);
}

int ReloadUserFolders(menu_list_t* menu_list)
{
    init_loading_screen("Loading folders...");

	if (menu_list->folders)
	{
		UnloadFolderList(menu_list->folders, menu_list->count);
		menu_list->count = 0;
		menu_list->folders = NULL;
	}

	if (menu_list->UpdatePath)
		menu_list->UpdatePath(menu_list->path);

	menu_list->folders = menu_list->ReadFolders(menu_list->path, &(menu_list->count));
	if (app_config.doSort)
		qsort(menu_list->folders, menu_list->count, sizeof(folder_entry_t), &qsortFolderList_Compare);

    stop_loading_screen();

	if (!menu_list->folders)
	{
		show_message("No folders found");
		return 0;
	}

	return menu_list->count;
}

char* _strSauceDetails(sauce_record_t* record)
{
	char* ret = malloc(1024);

    sprintf(ret, "Id: %s v%s\n"
		"Title: %s\n"
		"Author: %s\n"
		"Group: %s\n"
		"Date: %s\n"
		"Data Type: %s\n"
		"File Type: %d\n",
			record->ID, record->version,
			record->title,
			record->author,
			record->group,
			record->date,
			sauceStringDataType(record->dataType),
			record->fileType);

	if (record->flags)
		sprintf(ret + strlen(ret), "Flags: %d\n", record->flags);

	if (record->tinfo1)
		sprintf(ret + strlen(ret), "Tinfo 1: %d\n", record->tinfo1);

	if (record->tinfo2)
		sprintf(ret + strlen(ret), "Tinfo 2: %d\n", record->tinfo2);

	if (record->tinfo3)
		sprintf(ret + strlen(ret), "Tinfo 3: %d\n", record->tinfo3);

	if (record->tinfo4)
		sprintf(ret + strlen(ret), "Tinfo 4: %d\n", record->tinfo4);

	sprintf(ret + strlen(ret), "TinfoS: %s\n", record->tinfos);
	if (record->comments > 0)
	{
		sprintf(ret + strlen(ret), "Comments:\n");
		for (int32_t i = 0; i < record->comments; i++)
			sprintf(ret + strlen(ret), "%s\n", record->comment_lines[i]);
	}

	return ret;
}

void SetMenu(int id)
{   
	switch (menu_id) //Leaving menu
	{
		case MENU_MAIN_SCREEN: //Main Menu
		case MENU_BROWSE_USB: //USB Saves Menu
		case MENU_BROWSE_HDD: //HHD Saves Menu
		case MENU_ONLINE_DB: //Cheats Online Menu
		case MENU_MUSIC_LIST: //Backup Menu
		case MENU_SETTINGS: //Options Menu
		case MENU_CREDITS: //About Menu
		case MENU_ANSFILES: //Cheat Selection Menu
		case MENU_ONLINE_LIST:
			break;

		case MENU_DETAILS_VIEW: //Cheat View Menu
			if (app_config.doAni)
				Draw_CheatsMenu_View_Ani_Exit();
			break;

		case MENU_FILE_OPTIONS: //Cheat Option Menu
			if (app_config.doAni)
				Draw_CheatsMenu_Options_Ani_Exit();
			break;
	}
	
	switch (id) //going to menu
	{
		case MENU_MAIN_SCREEN: //Main Menu
			if (app_config.doAni || menu_id == MENU_MAIN_SCREEN) //if load animation
				Draw_MainMenu_Ani();
			break;

		case MENU_BROWSE_USB: //USB saves Menu
			if (!usb_folders.folders && !ReloadUserFolders(&usb_folders))
				return;
			
			if (app_config.doAni)
				Draw_UserCheatsMenu_Ani(&usb_folders);
			break;

		case MENU_BROWSE_HDD: //HDD saves Menu
			if (!hdd_folders.folders)
				ReloadUserFolders(&hdd_folders);
			
			if (app_config.doAni)
				Draw_UserCheatsMenu_Ani(&hdd_folders);
			break;

		case MENU_ONLINE_DB: //Cheats Online Menu
//TODO: Animated Year Menu
//			if (app_config.doAni)
//				Draw_UserCheatsMenu_Ani(selected_online_year);
			break;

		case MENU_CREDITS: //About Menu
			if (app_config.doAni)
				Draw_AboutMenu_Ani();
			break;

		case MENU_SETTINGS: //Options Menu
			if (app_config.doAni)
				Draw_OptionsMenu_Ani();
			break;

		case MENU_MUSIC_LIST: //User Backup Menu
			if (!music_folder.files)
			{
				ReadMusicFiles(&music_folder, music_files);
    			qsort(music_folder.files, music_folder.file_count, sizeof(file_entry_t), &qsortFileList_Compare);
			}
			selected_folder = &music_folder;
// ---------
			last_menu_id[MENU_ANSFILES] = 0;

			if (app_config.doAni)
				Draw_CheatsMenu_Selection_Ani();
			break;

		case MENU_ANSFILES: //Cheat Selection Menu
			//if entering from game list, don't keep index, otherwise keep
			if (menu_id == MENU_BROWSE_USB || menu_id == MENU_BROWSE_HDD || menu_id == MENU_ONLINE_DB)
				menu_old_sel[MENU_ANSFILES] = 0;

			if (app_config.doAni && menu_id != MENU_DETAILS_VIEW && menu_id != MENU_FILE_OPTIONS)
				Draw_CheatsMenu_Selection_Ani();
			break;

		case MENU_DETAILS_VIEW: //Cheat View Menu
			menu_old_sel[MENU_DETAILS_VIEW] = 0;
			if (app_config.doAni)
				Draw_CheatsMenu_View_Ani("File Details");
			break;

		case MENU_ONLINE_LIST: //Save Detail View Menu
			if (!selected_online_year->folders)
				ReloadUserFolders(selected_online_year);

			if (app_config.doAni)
				Draw_UserCheatsMenu_Ani(selected_online_year);
			break;

		case MENU_FILE_OPTIONS: //Cheat Option Menu
			menu_old_sel[MENU_FILE_OPTIONS] = 0;
			if (app_config.doAni)
				Draw_CheatsMenu_Options_Ani();
			break;
	}
	
	menu_old_sel[menu_id] = menu_sel;
	if (last_menu_id[menu_id] != id)
		last_menu_id[id] = menu_id;
	menu_id = id;
	
	menu_sel = menu_old_sel[menu_id];
}

void move_letter_back(folder_entry_t * games, int game_count)
{
	int i;
	char ch = toupper(games[menu_sel].name[0]);

	if ((ch > '\x29') && (ch < '\x40'))
	{
		menu_sel = 0;
		return;
	}

	for (i = menu_sel; (i > 0) && (ch == toupper(games[i].name[0])); i--) {}

	menu_sel = i;
}

void move_letter_fwd(folder_entry_t * games, int game_count)
{
	int i;
	char ch = toupper(games[menu_sel].name[0]);

	if (ch == 'Z')
	{
		menu_sel = game_count - 1;
		return;
	}
	
	for (i = menu_sel; (i < game_count - 2) && (ch == toupper(games[i].name[0])); i++) {}

	menu_sel = i;
}

void move_selection_back(int game_count, int steps)
{
	menu_sel -= steps;
	if ((menu_sel == -1) && (steps == 1))
		menu_sel = game_count - 1;
	else if (menu_sel < 0)
		menu_sel = 0;
}

void move_selection_fwd(int game_count, int steps)
{
	menu_sel += steps;
	if ((menu_sel == game_count) && (steps == 1))
		menu_sel = 0;
	else if (menu_sel >= game_count)
		menu_sel = game_count - 1;
}

void doFolderMenu(menu_list_t * menu_list)
{
    if(readPad(0))
    {
    	if(paddata[0].BTN_UP)
    		move_selection_back(menu_list->count, 1);
    
    	else if(paddata[0].BTN_DOWN)
    		move_selection_fwd(menu_list->count, 1);
    
    	else if (paddata[0].BTN_LEFT)
    		move_selection_back(menu_list->count, 5);
    
    	else if (paddata[0].BTN_L1)
    		move_selection_back(menu_list->count, 25);
    
    	else if (paddata[0].BTN_L2)
    		move_letter_back(menu_list->folders, menu_list->count);
    
    	else if (paddata[0].BTN_RIGHT)
    		move_selection_fwd(menu_list->count, 5);
    
    	else if (paddata[0].BTN_R1)
    		move_selection_fwd(menu_list->count, 25);
    
    	else if (paddata[0].BTN_R2)
    		move_letter_fwd(menu_list->folders, menu_list->count);
    
    	else if (paddata[0].BTN_CIRCLE)
    	{
    		SetMenu(MENU_MAIN_SCREEN);
    		return;
    	}
    	else if (paddata[0].BTN_CROSS)
    	{
    		if (!menu_list->folders[menu_sel].files)
			{
    			menu_list->ReadFiles(&menu_list->folders[menu_sel]);

				if (app_config.doSort)
					qsort(menu_list->folders[menu_sel].files, menu_list->folders[menu_sel].file_count, sizeof(file_entry_t), &qsortFileList_Compare);
			}

    		selected_folder = &menu_list->folders[menu_sel];
    		SetMenu(MENU_ANSFILES);
    		return;
    	}
/*
    	else if (paddata[0].BTN_TRIANGLE && menu_list->UpdatePath)
    	{
    		selected_folder = &menu_list->folders[menu_sel];
    		SetMenu(MENU_ONLINE_LIST);
    		return;
    	}
*/
		else if (paddata[0].BTN_SQUARE)
		{
			ReloadUserFolders(menu_list);
		}
	}

	Draw_UserCheatsMenu(menu_list);
}

void doMainMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].BTN_LEFT)
			move_selection_back(6, 1);

		else if(paddata[0].BTN_RIGHT)
			move_selection_fwd(6, 1);

		else if (paddata[0].BTN_CROSS)
		    SetMenu(menu_sel+1);

		else if(paddata[0].BTN_CIRCLE && show_dialog(1, "Exit to XMB?"))
			close_app = 1;
	}
	
	Draw_MainMenu();
}

void doAboutMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if (paddata[0].BTN_CIRCLE)
		{
			SetMenu(MENU_MAIN_SCREEN);
			return;
		}
	}

	Draw_AboutMenu();
}

void doOptionsMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].BTN_UP)
			move_selection_back(menu_options_maxopt, 1);

		else if(paddata[0].BTN_DOWN)
			move_selection_fwd(menu_options_maxopt, 1);

		else if (paddata[0].BTN_CIRCLE)
		{
			save_app_settings(&app_config);
			set_ttf_window(0, 0, 848 + app_config.marginH, 512 + app_config.marginV, WIN_SKIP_LF);
			SetMenu(MENU_MAIN_SCREEN);
			return;
		}
		else if (paddata[0].BTN_LEFT)
		{
			if (menu_options[menu_sel].type == APP_OPTION_LIST)
			{
				if (*menu_options[menu_sel].value > 0)
					(*menu_options[menu_sel].value)--;
				else
					*menu_options[menu_sel].value = menu_options_maxsel[menu_sel] - 1;
			}
			else if (menu_options[menu_sel].type == APP_OPTION_INC)
				(*menu_options[menu_sel].value)--;
			
			if (menu_options[menu_sel].type != APP_OPTION_CALL)
				menu_options[menu_sel].callback(*menu_options[menu_sel].value);
		}
		else if (paddata[0].BTN_RIGHT)
		{
			if (menu_options[menu_sel].type == APP_OPTION_LIST)
			{
				if (*menu_options[menu_sel].value < (menu_options_maxsel[menu_sel] - 1))
					*menu_options[menu_sel].value += 1;
				else
					*menu_options[menu_sel].value = 0;
			}
			else if (menu_options[menu_sel].type == APP_OPTION_INC)
				*menu_options[menu_sel].value += 1;

			if (menu_options[menu_sel].type != APP_OPTION_CALL)
				menu_options[menu_sel].callback(*menu_options[menu_sel].value);
		}
		else if (paddata[0].BTN_CROSS)
		{
			if (menu_options[menu_sel].type == APP_OPTION_BOOL)
				menu_options[menu_sel].callback(*menu_options[menu_sel].value);

			else if (menu_options[menu_sel].type == APP_OPTION_CALL)
				menu_options[menu_sel].callback(0);
		}
	}
	
	Draw_OptionsMenu();
}

void doFileViewMenu()
{
	//Calc max
	int max = 0;
	const char * str;

	if (!selected_file->details)
	{
		if (selected_file->sauce)
			selected_file->details = _strSauceDetails(selected_file->sauce);
		else
			asprintf(&selected_file->details, "File: %s%s\n", selected_folder->path, selected_file->file);
	}

	for(str = selected_file->details; *str; ++str)
		max += (*str == '\n');

	if (max <= 0)
		max = 1;
	
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].BTN_UP)
			move_selection_back(max, 1);

		else if(paddata[0].BTN_DOWN)
			move_selection_fwd(max, 1);

		else if (paddata[0].BTN_CIRCLE)
		{
			SetMenu(last_menu_id[MENU_DETAILS_VIEW]);
			return;
		}
	}
	
	Draw_CheatsMenu_View("File Details");
}

void displayAnsi()
{
	struct ansilove_ctx ctx;
	struct ansilove_options options;
	int (*loader)(struct ansilove_ctx *, struct ansilove_options *) = NULL;

	char in_file_path[256];

	if (selected_folder->flags & FOLDER_FLAG_FTP)
	{
		snprintf(in_file_path, sizeof(in_file_path), ANSIVIEW_LOCAL_CACHE "%s_%s", selected_folder->name, selected_file->file);

		if (file_exists(in_file_path) != SUCCESS)
		{
			char url_path[128];

			snprintf(url_path, sizeof(url_path), ONLINE_URL "%s%s/raw/%s", 
						selected_folder->path, selected_folder->name, selected_file->file);
			
			if (ftp_util(url_path, in_file_path) != SUCCESS)
			{
				LOG("ERROR: Failed to download (%s)", url_path);
				return;
			}
		}

		if (!selected_file->sauce)
			selected_file->sauce = sauceReadFileName(in_file_path);
	}
	else
		snprintf(in_file_path, sizeof(in_file_path), "%s%s", selected_folder->path, selected_file->file);

	LOG("Loading '%s'...", in_file_path);

	loader = ansi_loaders[selected_file->type];

	ansilove_init(&ctx, &options);
	ansilove_loadfile(&ctx, in_file_path);

	/* adjust the file size if file contains a SAUCE record */
	if (selected_file->sauce)
	{
		ctx.length -= 129 - (selected_file->sauce->comments > 0 ? 5 + 64 * selected_file->sauce->comments : 0);

		/* set icecolors to true if defined in SAUCE record ANSiFlags */
		if (selected_file->sauce->flags & 1)
			options.icecolors = true;
	}

	if ((selected_file->type == ANSILOVE_FILETYPE_BIN) && (selected_file->options))
	{
		options.columns = selected_file->options[0].value[selected_file->options[0].sel][0];
		LOG("Set columns = %d", options.columns);
	}

//	options.diz = true;

	if (loader(&ctx, &options) == -1)
	{
		LOG("Loader Error: (type=%d) %s", selected_file->type, ansilove_error(&ctx));
		ansilove_clean(&ctx);
		return;
	}

	png_texture tex;

	memcpy(free_mem, ctx.png.buffer, ctx.png.length);
	tex.texture.width = ctx.png.sx;
	tex.texture.height = ctx.png.sy;
	tex.texture.pitch = ctx.png.sx * 4;
	tex.texture_off = tiny3d_TextureOffset(free_mem);      // get the offset (RSX use offset instead address)

	if (app_config.exportPNG)
	{
		snprintf(in_file_path, sizeof(in_file_path), "/dev_hdd0/tmp/%s.png", selected_file->file);
		LOG("Saving '%s'...", in_file_path);
//		ansilove_savefile(&ctx, "example.png");
	}

	ansilove_clean(&ctx);

	int ys = tex.texture.height;
	do
	{
		tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
		tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
		tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
		TINY3D_BLEND_FUNC_SRC_ALPHA_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_RGB_ONE_MINUS_SRC_ALPHA,
		TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

		tiny3d_Project2D();

		DrawBackground2D(0);

		//Loading animation
		DrawTextureCenteredX(&tex, 424, ys - tex.texture.height, 0, tex.texture.width, tex.texture.height, 0xFFFFFFFF);

		tiny3d_Flip();

		readPad(0);
	}
	while ((ys-- > 0) && !paddata[0].BTN_CIRCLE);

	paddata[0].BTN_CIRCLE = 0;

	return;
}

void doCodeOptionsMenu()
{
    file_entry_t* code = &selected_folder->files[menu_old_sel[last_menu_id[MENU_FILE_OPTIONS]]];
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].BTN_UP)
			move_selection_back(selected_file->options[option_index].size, 1);

		else if(paddata[0].BTN_DOWN)
			move_selection_fwd(selected_file->options[option_index].size, 1);

		else if (paddata[0].BTN_CIRCLE)
		{
			code->activated = 0;
			SetMenu(last_menu_id[MENU_FILE_OPTIONS]);
			return;
		}
		else if (paddata[0].BTN_CROSS)
		{
			code->options[option_index].sel = menu_sel;
//			const char* codecmd = code->options[option_index].value[menu_sel];

			displayAnsi();

			option_index++;
			
			if (option_index >= code->options_count)
			{
				SetMenu(last_menu_id[MENU_FILE_OPTIONS]);
				return;
			}
			else
				menu_sel = 0;
		}
	}
	
	Draw_CheatsMenu_Options();
}

void doOnlineYearsMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].BTN_UP)
			move_selection_back(total_years, 1);

		else if(paddata[0].BTN_DOWN)
			move_selection_fwd(total_years, 1);

		else if (paddata[0].BTN_LEFT)
			move_selection_back(total_years, 5);

		else if (paddata[0].BTN_RIGHT)
			move_selection_fwd(total_years, 5);

		else if (paddata[0].BTN_CROSS)
		{
			selected_online_year = &online_folders[menu_sel];

			SetMenu(MENU_ONLINE_LIST);
			return;
		}
		else if (paddata[0].BTN_CIRCLE)
		{
			SetMenu(MENU_MAIN_SCREEN);
			return;
		}
	}

	Draw_OnlineYearsMenu(online_folders, total_years);
}

void music_load(list_node_t* item);

void music_init()
{
    MikMod_InitThreads();

	/* register all the drivers */
    MikMod_RegisterDriver(&drv_psl1ght);    

	/* register all the module loaders */
	MikMod_RegisterAllLoaders();

	/* initialize the library */
	md_mode |= DMODE_SOFT_MUSIC | DMODE_STEREO | DMODE_HQMIXER | DMODE_16BITS;
	if (MikMod_Init("")) {
		LOG("Could not initialize sound, reason: %s\n",	MikMod_strerror(MikMod_errno));
		return;
	}

	LOG("Init %s", MikMod_InfoDriver());
	LOG("Loader %s", MikMod_InfoLoader());

	music_files = list_alloc();
	if (LoadMusicList(music_files) == 0)
		return;

	music_load(music_files->head);
}

void music_update_thread(void* data)
{
	list_node_t* item = (list_node_t*) data;

	while (Player_Active()) {
		/* we're playing */
		usleep(10000);
		MikMod_Update();
	}

	Player_Stop();
	Player_Free(module);

	music_load(item->next);
}

void music_load(list_node_t* item)
{
	char mod_file[256];
	char* filename;

	if (!item)
	{
		LOG("[!] Empty Playlist item");
		item = music_files->head;
	}

	filename = list_get(item);
	snprintf(mod_file, sizeof(mod_file), ANSIVIEW_MUSIC_PATH "%s", filename);
	LOG("Loading %s ...", mod_file);

	/* load module */
	module = Player_Load(mod_file, 64, 0);

	if (module)
	{
		sys_ppu_thread_t tid;

		/* start module */
		Player_Start(module);
		LOG("Playing %s ...", module->songname);

		int ret = sysThreadCreate (&tid, music_update_thread, (void*) item, 1000, 16*1024, THREAD_JOINABLE, "music_upd");

		if (ret < 0)
		{
			LOG("Thread Error: %X", ret);
			/* give up */
			MikMod_Exit();
			return;
		}
	}
	else
	{
		LOG("Could not load module, reason: %s\n", MikMod_strerror(MikMod_errno));
		music_load(item->next);
	}

	return;
}

void free_music_list()
{
	list_node_t *node = list_head(music_files);

	while (node)
	{
		free(list_get(node));
		node = node->next;
	}

	list_free(music_files);
	return;
}

void doPatchMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].BTN_UP)
			move_selection_back(selected_folder->file_count, 1);

		else if(paddata[0].BTN_DOWN)
			move_selection_fwd(selected_folder->file_count, 1);

		else if (paddata[0].BTN_LEFT)
			move_selection_back(selected_folder->file_count, 5);

		else if (paddata[0].BTN_RIGHT)
			move_selection_fwd(selected_folder->file_count, 5);

		else if (paddata[0].BTN_CIRCLE)
		{
			SetMenu(last_menu_id[MENU_ANSFILES]);
			return;
		}
		else if (paddata[0].BTN_CROSS)
		{
			selected_file = &selected_folder->files[menu_sel];
			selected_file->activated = !selected_file->activated;

			if (selected_folder->files[menu_sel].activated)
			{
				//Check if option code
				if (!selected_folder->files[menu_sel].options)
				{
//					int size;
//					selected_folder->files[menu_sel].options = ReadOptions(selected_folder->files[menu_sel], &size);
//					selected_folder->files[menu_sel].options_count = size;
				}
				
				if (selected_folder->files[menu_sel].options)
				{
					selected_file = &selected_folder->files[menu_sel];
					option_index = 0;
					SetMenu(MENU_FILE_OPTIONS);
					return;
				}

				if ((selected_file->type > 0) && (selected_file->type < ANSILOVE_FILE_TYPES))
				{
					displayAnsi();

					selected_folder->files[menu_sel].activated = 0;
					return;
				}

			}
		}
		else if (paddata[0].BTN_TRIANGLE)
		{
			selected_file = &selected_folder->files[menu_sel];

			SetMenu(MENU_DETAILS_VIEW);
			return;
		}
	}
	
	Draw_CheatsMenu_Selection(menu_sel, 0xFFFFFFFF);
}

// Resets new frame
void drawScene()
{
	switch (menu_id)
	{
		case MENU_MAIN_SCREEN:
			doMainMenu();
			break;

		case MENU_BROWSE_USB: //USB Saves Menu
			doFolderMenu(&usb_folders);
			break;

		case MENU_BROWSE_HDD: //HDD Saves Menu
			doFolderMenu(&hdd_folders);
			break;

		case MENU_ONLINE_DB: //Online Cheats Menu
			doOnlineYearsMenu();
			break;

		case MENU_CREDITS: //About Menu
			doAboutMenu();
			break;

		case MENU_SETTINGS: //Options Menu
			doOptionsMenu();
			break;

		case MENU_MUSIC_LIST: //User Backup Menu
		case MENU_ANSFILES: //Cheats Selection Menu
			doPatchMenu();
			break;

		case MENU_DETAILS_VIEW: //Cheat View Menu
			doFileViewMenu();
			break;

		case MENU_FILE_OPTIONS: //Cheat Option Menu
			doCodeOptionsMenu();
			break;

		case MENU_ONLINE_LIST: //Save Details Menu
			doFolderMenu(selected_online_year);
			break;
	}
}

void exiting()
{
	http_end();
	sysModuleUnload(SYSMODULE_PNGDEC);
}

void registerSpecialChars()
{
	/* Register save tags
	RegisterSpecialCharacter(CHAR_TAG_PS1, 2, 1.5, &menu_textures[tag_ps1_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS2, 2, 1.5, &menu_textures[tag_ps2_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS3, 2, 1.5, &menu_textures[tag_ps3_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSP, 2, 1.5, &menu_textures[tag_psp_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSV, 2, 1.5, &menu_textures[tag_psv_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PCE, 2, 1.5, &menu_textures[tag_pce_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_LOCKED, 0, 1.5, &menu_textures[tag_lock_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_OWNER, 0, 1.5, &menu_textures[tag_own_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_WARNING, 0, 1.5, &menu_textures[tag_warning_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_APPLY, 0, 1.0, &menu_textures[tag_apply_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_ZIP, 0, 1.2, &menu_textures[tag_zip_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_TRANSFER, 0, 1.2, &menu_textures[tag_transfer_png_index]);
*/
	// Register button icons
	RegisterSpecialCharacter(CHAR_BTN_X, 0, 1.2, &menu_textures[footer_ico_cross_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_S, 0, 1.2, &menu_textures[footer_ico_square_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_T, 0, 1.2, &menu_textures[footer_ico_triangle_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_O, 0, 1.2, &menu_textures[footer_ico_circle_png_index]);
}

void _init_16colors_years()
{
	online_folders = malloc(sizeof(menu_list_t) * total_years);

	for (int i = 0; i < total_years; i++)
	{
		online_folders[i].icon_id = cat_db_png_index;
		online_folders[i].title = "Browse 16colors";
		online_folders[i].folders = NULL;
		online_folders[i].count = 0;
		online_folders[i].ReadFolders = &ReadOnlineList;
		online_folders[i].ReadFiles = &ReadOnlineFiles;
		online_folders[i].UpdatePath = NULL;
		snprintf(online_folders[i].path, sizeof(online_folders[i].path), ONLINE_URL "%d/", (1990+i));
	}
}

/*
	Program start
*/
s32 main(s32 argc, const char* argv[])
{
	dbglogger_init();
//	dbglogger_failsafe("9999");

	http_init();

	tiny3d_Init(1024*1024);

	ioPadInit(7);
	
	sysModuleLoad(SYSMODULE_PNGDEC);

	atexit(exiting); // Tiny3D register the event 3 and do exit() call when you exit  to the menu

	// register exit callback
	if(sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL)==0) inited |= INITED_CALLBACK;
	
	// Load textures
	LoadTextures_Menu();

	// Unpack application data on first run
//	if (file_exists(ANSIVIEW_LOCAL_CACHE "appdata.zip") == SUCCESS)
//		_unzip_app_data();

	// Splash screen logo (fade-in)
	drawSplashLogo(1);

	// Load application settings
	load_app_settings(&app_config);

	// Setup font
	SetExtraSpace(5);
	SetCurrentFont(0);

	registerSpecialChars();

	menu_options_maxopt = 0;
	while (menu_options[menu_options_maxopt].name)
		menu_options_maxopt++;
	
	menu_options_maxsel = (int *)calloc(1, menu_options_maxopt * sizeof(int));
	
	int i = 0;
	for (i = 0; i < menu_options_maxopt; i++)
	{
		menu_options_maxsel[i] = 0;
		if (menu_options[i].type == APP_OPTION_LIST)
		{
			while (menu_options[i].options[menu_options_maxsel[i]])
				menu_options_maxsel[i]++;
		}
	}

	// Splash screen logo (fade-out)
	drawSplashLogo(-1);

	music_init();
	
	//Set options
	music_callback(!app_config.music);
	update_callback(!app_config.update);

	_init_16colors_years();

	SetMenu(MENU_MAIN_SCREEN);

	while (!close_app)
	{
		tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

		// Enable alpha Test
		tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

		// Enable alpha blending.
		tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
			TINY3D_BLEND_FUNC_SRC_ALPHA_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_RGB_ONE_MINUS_SRC_ALPHA,
			TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
		
		// change to 2D context (remember you it works with 848 x 512 as virtual coordinates)
		tiny3d_Project2D();

		drawScene();

#ifdef DEBUG_ENABLE_LOGGING
		if(paddata[0].BTN_SELECT)
		{
			LOG("Screen");
			dbglogger_screenshot_tmp(0);
			LOG("Shot");
		}
#endif

		//Draw help
		if (menu_pad_help[menu_id])
		{
			u8 alpha = 0xFF;
			if (idle_time > 80)
			{
				int dec = (idle_time - 80) * 4;
				if (dec > alpha)
					dec = alpha;
				alpha -= dec;
			}
			
			SetFontSize(APP_FONT_SIZE_DESCRIPTION);
			SetCurrentFont(0);
			SetFontAlign(1);
			SetFontColor(APP_FONT_COLOR | alpha, 0);
			DrawString(0, 480, (char *)menu_pad_help[menu_id]);
			SetFontAlign(0);
		}
		
		tiny3d_Flip();
	}
	
	return 0;
}
