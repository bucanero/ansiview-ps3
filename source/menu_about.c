#include <unistd.h>
#include <string.h>
#include <pngdec/pngdec.h>

#include "files.h"
#include "menu.h"
#include "menu_about.h"

#include <tiny3d.h>
#include <libfont.h>

void _draw_AboutMenu(u8 alpha)
{
	int cnt = 0;
    
    //------------- About Menu Contents
//	DrawTextureCenteredX(&menu_textures[logo_text_png_index], 424, 70, 0, 245, 40, 0xFFFFFF00 | alpha);

    SetFontAlign(1);
	SetCurrentFont(font_adonais_regular);
	SetFontColor(APP_FONT_MENU_COLOR | alpha, 0);
	SetFontSize(APP_FONT_SIZE_ABOUT);
	DrawStringMono(0, 235, "ANSi View (PS3)");
    
    for (cnt = 0; menu_about_strings[cnt] != NULL; cnt += 2)
    {
        SetFontAlign(2);
		DrawStringMono((848 / 2) - 10, 300 + (cnt * 14), menu_about_strings[cnt]);
        
		SetFontAlign(0);
		DrawStringMono((848 / 2) + 10, 300 + (cnt * 14), menu_about_strings[cnt + 1]);
    }

	SetFontAlign(1);
	DrawStringMono(0, 440, "www.bucanero.com.ar");
	SetFontAlign(0);
}

void Draw_AboutMenu_Ani()
{
	int ani = 0;
	for (ani = 0; ani < MENU_ANI_MAX; ani++)
	{
		tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
		tiny3d_AlphaTest(1, 0x0, TINY3D_ALPHA_FUNC_GEQUAL);
		tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
			0x00000303 | 0x00000000,
			TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

		tiny3d_Project2D();

		DrawHeader_Ani(cat_about_png_index, "About", "v" ANSIVIEW_VERSION, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

		//------------- About Menu Contents

		int rate = (0x100 / (MENU_ANI_MAX - 0x60));
		u8 about_a = (u8)((((ani - 0x60) * rate) > 0xFF) ? 0xFF : ((ani - 0x60) * rate));
		if (ani < 0x60)
			about_a = 0;

		_draw_AboutMenu(about_a);

		tiny3d_Flip();

		if (about_a == 0xFF)
			return;
	}
}

void Draw_AboutMenu()
{
	DrawHeader(cat_about_png_index, 0, "About", "v" ANSIVIEW_VERSION, APP_FONT_TITLE_COLOR | 0xFF, 0xffffffff, 0);
	_draw_AboutMenu(0xFF);
}
