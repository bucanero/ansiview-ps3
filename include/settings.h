
#define ANSIVIEW_VERSION		"0.5.0"		//ANSi View PS3 version (about menu)

#define MENU_TITLE_OFF			30			//Offset of menu title text from menu mini icon
#define MENU_ICON_OFF 			70          //X Offset to start printing menu mini icon
#define MENU_ANI_MAX 			0x80        //Max animation number
#define MENU_SPLIT_OFF			200			//Offset from left of sub/split menu to start drawing
#define OPTION_ITEM_OFF         730         //Offset from left of settings item/value

enum app_option_type
{
    APP_OPTION_NONE,
    APP_OPTION_BOOL,
    APP_OPTION_LIST,
    APP_OPTION_INC,
    APP_OPTION_CALL,
};

typedef struct
{
	char * name;
	char * * options;
	int type;
	uint8_t * value;
	void(*callback)(int);
} menu_option_t;

typedef struct
{
    uint8_t music;
    uint8_t doSort;
    uint8_t doAni;
    uint8_t marginH;
    uint8_t marginV;
    uint8_t update;
    uint8_t exportPNG;
    uint8_t autoScroll;
} app_config_t;

extern const menu_option_t menu_options[];

extern app_config_t app_config;
