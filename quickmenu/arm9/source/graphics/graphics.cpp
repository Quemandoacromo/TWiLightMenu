/*-----------------------------------------------------------------
 Copyright (C) 2015
	Matthew Scholefield

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <maxmod9.h>
#include <gl2d.h>
#include "bios_decompress_callback.h"
#include "myDSiMode.h"
#include "fileCopy.h"
#include "common/flashcard.h"
#include "common/systemdetails.h"
#include "common/twlmenusettings.h"
#include <cmath>

// Graphic files
#include "cursor.h"
#include "iconbox.h"
#include "iconbox_pressed.h"
#include "wirelessicons.h"
#include "pictodlp.h"
#include "pictodlp_selected.h"
#include "icon_dscard.h"
#include "icon_gba.h"
#include "iconPhat_gba.h"
#include "icon_gbamode.h"
#include "cornericons.h"
#include "icon_settings.h"
#include "icon_settings_away.h"

#include "cursorpal.h"
#include "usercolors.h"

#include "../iconTitle.h"
#include "graphics.h"
#include "common/lodepng.h"
#include "color.h"
#include "fontHandler.h"
#include "../ndsheaderbanner.h"
#include "../errorScreen.h"
#include "../date.h"

#define CONSOLE_SCREEN_WIDTH 32
#define CONSOLE_SCREEN_HEIGHT 24

#define CONVERT_COLOR(r,g,b) r>>3 | (g>>3)<<5 | (b>>3)<<10 | BIT(15)

extern bool useTwlCfg;

//extern bool widescreenEffects;

extern bool whiteScreen;
extern bool fadeType;
extern bool fadeSpeed;
extern bool controlTopBright;
extern bool controlBottomBright;
int fadeDelay = 0;

extern int colorRvalue;
extern int colorGvalue;
extern int colorBvalue;

int screenBrightness = 31;

int frameOf60fps = 60;
int frameDelay = 0;
bool frameDelayEven = true; // For 24FPS
bool renderFrame = true;

bool showProgressBar = false;
int progressBarLength = 0;

float cursorTargetTL = 0.0f, cursorTargetTR = 0.0f, cursorTargetBL = 0.0f, cursorTargetBR = 0.0f;
float cursorTL = 0.0f, cursorTR = 0.0f, cursorBL = 0.0f, cursorBR = 0.0f;

extern int spawnedtitleboxes;

extern bool showCursor;
extern bool startMenu;
extern MenuEntry cursorPosition;

extern MenuEntry initialTouchedPosition;
extern MenuEntry currentTouchedPosition;

extern bool pictochatFound;
extern bool dlplayFound;
extern bool gbaBiosFound;
extern bool cardEjected;

int boxArtType[2] = {0};

bool moveIconUp[7] = {false};
int iconYpos[7] = {25, 73, 73, 121, 175, 170, 175};

bool showdialogbox = false;
int dialogboxHeight = 0;

constexpr int calendarXPos = 127;
constexpr int calendarYPos = 31;
constexpr int clockXPos = 15;
constexpr int clockYPos = 47;

int cursorTexID, iconboxTexID, iconboxPressedTexID, wirelessiconTexID, pictodlpTexID, pictodlpSelectedTexID, dscardiconTexID, gbaiconTexID, cornericonTexID, settingsiconTexID, settingsiconAwayTexID;

glImage cursorImage[(32 / 32) * (128 / 32)];
glImage iconboxImage[(256 / 16) * (128 / 64)];
glImage iconboxPressedImage[(256 / 16) * (64 / 64)];
glImage wirelessIcons[(32 / 32) * (64 / 32)];
glImage pictodlpImage[(128 / 16) * (256 / 64)];
glImage pictodlpSelectedImage[(128 / 16) * (128 / 64)];
glImage dscardIconImage[(32 / 32) * (64 / 32)];
glImage gbaIconImage[(32 / 32) * (64 / 32)];
glImage cornerIcons[(32 / 32) * (128 / 32)];
glImage settingsIconImage[(32 / 32) * (64 / 32)];
glImage settingsIconAwayImage[(32 / 32) * (32 / 32)];

u16 bmpImageBuffer[256*192] = {0};
u16 topImageBuffer[256*192] = {0};

u16 calendarImageBuffer[113*113] = {0};
u16 calendarBigImageBuffer[113*129] = {0};
u16 markerImageBuffer[13*13] = {0};

u16 clockImageBuffer[97*97] = {0};
u16 clockNeedleColor;
u16 clockPinColor;

u16* colorTable = NULL;

void vramcpy_ui (void* dest, const void* src, int size) 
{
	u16* destination = (u16*)dest;
	u16* source = (u16*)src;
	while (size > 0) {
		*destination++ = *source++;
		size-=2;
	}
}

void ClearBrightness(void) {
	fadeType = true;
	screenBrightness = 0;
	swiWaitForVBlank();
	swiWaitForVBlank();
}

bool screenFadedIn(void) { return (screenBrightness == 0); }

bool screenFadedOut(void) { return (screenBrightness > 24); }

void updateCursorTargetPos(void) { 
	switch (cursorPosition) {
		case MenuEntry::INVALID:
			cursorTargetTL = 0.0f;
			cursorTargetBL = 0.0f;
			cursorTargetTR = 0.0f;
			cursorTargetBR = 0.0f;
			break;
		case MenuEntry::CART:
			cursorTargetTL = 31.0f;
			cursorTargetBL = 23.0f;
			cursorTargetTR = 213.0f;
			cursorTargetBR = 61.0f;
			//drawCursorRect(31, 23, 213, 61);
			break;
		case MenuEntry::PICTOCHAT:
			cursorTargetTL = 31.0f;
			cursorTargetBL = 71.0f;
			cursorTargetTR = 117.0f;
			cursorTargetBR = 109.0f;
			//drawCursorRect(31, 71, 117, 109);
			break;
		case MenuEntry::DOWNLOADPLAY:
			cursorTargetTL = 127.0f;
			cursorTargetBL = 71.0f;
			cursorTargetTR = 213.0f;
			cursorTargetBR = 109.0f;
			//drawCursorRect(127, 71, 213, 109);
			break;
		case MenuEntry::GBA:
			cursorTargetTL = 31.0f;
			cursorTargetBL = 119.0f;
			cursorTargetTR = 213.0f;
			cursorTargetBR = 157.0f;
			//drawCursorRect(31, 119, 213, 157);
			break;
		case MenuEntry::BRIGHTNESS:
			cursorTargetTL = 0.0f;
			cursorTargetBL = 167.0f;
			cursorTargetTR = 20.0f;
			cursorTargetBR = 182.0f;
			//drawCursorRect(0, 167, 20, 182);
			break;
		case MenuEntry::SETTINGS:
			cursorTargetTL = 112.0f;
			cursorTargetBL = 167.0f;
			cursorTargetTR = 132.0f;
			cursorTargetBR = 182.0f;
			//drawCursorRect(112, 167, 132, 182);
			break;
		case MenuEntry::MANUAL:
			cursorTargetTL = 225.0f;
			cursorTargetBL = 167.0f;
			cursorTargetTR = 245.0f;
			cursorTargetBR = 182.0f;
			//drawCursorRect(225, 167, 245, 182);
			break;
	}
}

// Ported from PAlib (obsolete)
void SetBrightness(u8 screen, s8 bright) {
	u16 mode = 1 << 14;

	if (bright < 0) {
		mode = 2 << 14;
		bright = -bright;
	}
	if (bright > 31) bright = 31;
	*(vu16*)(0x0400006C + (0x1000 * screen)) = bright + mode;
}

void frameRateHandler(void) {
	frameOf60fps++;
	if (frameOf60fps > 60) frameOf60fps = 1;

	if (!renderFrame) {
		frameDelay++;
		switch (ms().fps) {
			case 11:
				renderFrame = (frameDelay == 5+frameDelayEven);
				break;
			case 24:
			//case 25:
				renderFrame = (frameDelay == 2+frameDelayEven);
				break;
			case 48:
				renderFrame = (frameOf60fps != 3
							&& frameOf60fps != 8
							&& frameOf60fps != 13
							&& frameOf60fps != 18
							&& frameOf60fps != 23
							&& frameOf60fps != 28
							&& frameOf60fps != 33
							&& frameOf60fps != 38
							&& frameOf60fps != 43
							&& frameOf60fps != 48
							&& frameOf60fps != 53
							&& frameOf60fps != 58);
				break;
			case 50:
				renderFrame = (frameOf60fps != 3
							&& frameOf60fps != 9
							&& frameOf60fps != 16
							&& frameOf60fps != 22
							&& frameOf60fps != 28
							&& frameOf60fps != 34
							&& frameOf60fps != 40
							&& frameOf60fps != 46
							&& frameOf60fps != 51
							&& frameOf60fps != 58);
				break;
			default:
				renderFrame = (frameDelay == 60/ms().fps);
				break;
		}
	}
}

//-------------------------------------------------------
// set up a 2D layer construced of bitmap sprites
// this holds the image when rendering to the top screen
//-------------------------------------------------------

void initSubSprites(void)
{

	oamInit(&oamSub, SpriteMapping_Bmp_2D_256, false);
	int id = 0;

	//set up a 4x3 grid of 64x64 sprites to cover the screen
	for (int y = 0; y < 3; y++)
		for (int x = 0; x < 4; x++) {
			oamSub.oamMemory[id].attribute[0] = ATTR0_BMP | ATTR0_SQUARE | (64 * y);
			oamSub.oamMemory[id].attribute[1] = ATTR1_SIZE_64 | (64 * x);
			oamSub.oamMemory[id].attribute[2] = ATTR2_ALPHA(1) | (8 * 32 * y) | (8 * x);
			++id;
		}

	swiWaitForVBlank();

	oamUpdate(&oamSub);
}

int getFavoriteColor(void) {
	int favoriteColor = (int)(useTwlCfg ? *(u8*)0x02000444 : PersonalData->theme);
	if (favoriteColor < 0 || favoriteColor >= 16) favoriteColor = 0; // Invalid color found, so default to gray
	return favoriteColor;
}

/* u16 convertVramColorToGrayscale(u16 val) {
	u8 b,g,r,max,min;
	b = ((val)>>10)&31;
	g = ((val)>>5)&31;
	r = (val)&31;
	// Value decomposition of hsv
	max = (b > g) ? b : g;
	max = (max > r) ? max : r;

	// Desaturate
	min = (b < g) ? b : g;
	min = (min < r) ? min : r;
	max = (max + min) / 2;

	return 32768|(max<<10)|(max<<5)|(max);
} */

void bottomBgLoad() {
	std::string bottomBGFile = "nitro:/graphics/bottombg.png";

	char temp[256];

	switch (ms().theme) {
		case TWLSettings::EThemeDSi: // DSi Theme
			sprintf(temp, "%s:/_nds/TwilightMenu/dsimenu/themes/%s/quickmenu/bottombg.png", sdFound() ? "sd" : "fat", ms().dsi_theme.c_str());
			break;
		case TWLSettings::ETheme3DS:
			sprintf(temp, "%s:/_nds/TwilightMenu/3dsmenu/themes/%s/quickmenu/bottombg.png", sdFound() ? "sd" : "fat", ms()._3ds_theme.c_str());
			break;
		case TWLSettings::EThemeR4:
			sprintf(temp, "%s:/_nds/TwilightMenu/r4menu/themes/%s/quickmenu/bottombg.png", sdFound() ? "sd" : "fat", ms().r4_theme.c_str());
			break;
		case TWLSettings::EThemeWood:
			// sprintf(temp, "%s:/_nds/TwilightMenu/akmenu/themes/%s/quickmenu/bottombg.png", sdFound() ? "sd" : "fat", ms().ak_theme.c_str());
			break;
		case TWLSettings::EThemeSaturn:
			sprintf(temp, "nitro:/graphics/bottombg_saturn.png");
			break;
		case TWLSettings::EThemeHBL:
		case TWLSettings::EThemeGBC:
			break;
	}

	if (access(temp, F_OK) == 0)
		bottomBGFile = std::string(temp);

	std::vector<unsigned char> image;
	uint imageWidth, imageHeight;
	lodepng::decode(image, imageWidth, imageHeight, bottomBGFile);
	if (imageWidth > 256 || imageHeight > 192)	return;

	for (uint i=0;i<image.size()/4;i++) {
		bmpImageBuffer[i] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
		if (colorTable) {
			bmpImageBuffer[i] = colorTable[bmpImageBuffer[i]];
		}
	}

	// Start loading
	u16* src = bmpImageBuffer;
	int x = 0;
	int y = 0;
	for (int i=0; i<256*192; i++) {
		if (x >= 256) {
			x = 0;
			y++;
		}
		BG_GFX[y*256+x] = *(src++);
		x++;
	}
}

// No longer used.
// void drawBG(glImage *images)
// {
// 	for (int y = 0; y < 256 / 16; y++)
// 	{
// 		for (int x = 0; x < 256 / 16; x++)
// 		{
// 			int i = y * 16 + x;
// 			glSprite(x * 16, y * 16, GL_FLIP_NONE, &images[i & 255]);
// 		}
// 	}
// }

auto getMenuEntryTexture(MenuEntry entry) {
	switch(entry) {
		case MenuEntry::CART:
			if(isDSiMode() && cardEjected)
				return &iconboxImage[1];
			if(initialTouchedPosition == MenuEntry::CART) {
				if(currentTouchedPosition != MenuEntry::CART)
					return &iconboxImage[1];
				return iconboxPressedImage;
			}
			return &iconboxImage[0];
		case MenuEntry::PICTOCHAT:
			if(!pictochatFound)
				return &pictodlpImage[1];
			if(initialTouchedPosition == MenuEntry::PICTOCHAT) {
				if(currentTouchedPosition != MenuEntry::PICTOCHAT)
					return &pictodlpImage[1];
				return &pictodlpSelectedImage[0];
			}
			return &pictodlpImage[0];
		case MenuEntry::DOWNLOADPLAY:
			if(!dlplayFound)
				return &pictodlpImage[3];
			if(initialTouchedPosition == MenuEntry::DOWNLOADPLAY) {
				if(currentTouchedPosition != MenuEntry::DOWNLOADPLAY)
					return &pictodlpImage[3];
				return &pictodlpSelectedImage[1];
			}
			return &pictodlpImage[2];
		case MenuEntry::GBA:
		{
			bool hasGbaCart = sys().isRegularDS() && (((u8*)GBAROM)[0xB2] == 0x96);
			if(hasGbaCart || sdFound()) {
				if(initialTouchedPosition == MenuEntry::GBA) {
					if(currentTouchedPosition != MenuEntry::GBA)
						return &iconboxImage[1];
					return iconboxPressedImage;
				}
				return &iconboxImage[0];
			}
			return &iconboxImage[1];
		}
		case MenuEntry::BRIGHTNESS:
			return &cornerIcons[0];
		case MenuEntry::SETTINGS:
			if(initialTouchedPosition == MenuEntry::SETTINGS) {
				if(currentTouchedPosition != MenuEntry::SETTINGS)
					return &settingsIconAwayImage[0];
				return &settingsIconImage[1];
			}
			return &settingsIconImage[0];
		case MenuEntry::MANUAL:
			return &cornerIcons[3];
		case MenuEntry::INVALID:
			break;
	}
	__builtin_unreachable();
}

void vBlankHandler()
{
	if (fadeType == true) {
		if (!fadeDelay) {
			screenBrightness--;
			if (screenBrightness < 0) screenBrightness = 0;
		}
		if (!fadeSpeed) {
			fadeDelay++;
			if (fadeDelay == 3) fadeDelay = 0;
		} else {
			fadeDelay = 0;
		}
	} else {
		if (!fadeDelay) {
			screenBrightness++;
			if (screenBrightness > 31) screenBrightness = 31;
		}
		if (!fadeSpeed) {
			fadeDelay++;
			if (fadeDelay == 3) fadeDelay = 0;
		} else {
			fadeDelay = 0;
		}
	}

	constexpr float swiftness = 0.25f;
	cursorTL += (cursorTargetTL - cursorTL) * swiftness;
	cursorBL += (cursorTargetBL - cursorBL) * swiftness;
	cursorTR += (cursorTargetTR - cursorTR) * swiftness;
	cursorBR += (cursorTargetBR - cursorBR) * swiftness;

	if (renderFrame) {
	  glBegin2D();
	  {
		if (controlBottomBright) SetBrightness(0, screenBrightness);
		if (controlTopBright && !ms().macroMode) SetBrightness(1, screenBrightness);

		// glColor(RGB15(31, 31-(3*ms().blfLevel), 31-(6*ms().blfLevel)));
		glColor(RGB15(31, 31, 31));
		if (whiteScreen) {
			glBoxFilled(0, 0, 256, 192, RGB15(31, 31, 31));
			if (showProgressBar) {
				int barXpos = 31;
				int barYpos = 169;
				glBoxFilled(barXpos, barYpos, barXpos+192, barYpos+5, RGB15(23, 23, 23));
				if (progressBarLength > 0) {
					glBoxFilled(barXpos, barYpos, barXpos+progressBarLength, barYpos+5, RGB15(0, 0, 31));
				}
			}
		} else if (startMenu) {
			glSprite(33, iconYpos[0], GL_FLIP_NONE, getMenuEntryTexture(MenuEntry::CART));
			if (isDSiMode() && cardEjected) {
				//glSprite(33, iconYpos[0], GL_FLIP_NONE, &iconboxImage[(REG_SCFG_MC == 0x11) ? 1 : 0]);
				//glSprite(40, iconYpos[0]+6, GL_FLIP_NONE, &dscardIconImage[(REG_SCFG_MC == 0x11) ? 1 : 0]);
				glSprite(40, iconYpos[0]+6, GL_FLIP_NONE, &dscardIconImage[1]);
			} else {
				if ((isDSiMode() && !flashcardFound() && sys().arm7SCFGLocked()) || (io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA)) {
					glSprite(40, iconYpos[0]+6, GL_FLIP_NONE, &dscardIconImage[0]);
				} else drawIcon(1, 40, iconYpos[0]+6);
				if (bnrWirelessIcon[1] > 0) glSprite(207, iconYpos[0]+30, GL_FLIP_NONE, &wirelessIcons[(bnrWirelessIcon[0]-1) & 31]);
			}
			// Playback animated icon
			if (bnriconisDSi[0]==true) {
				playBannerSequence(0);
			}
			glSprite(33, iconYpos[1], GL_FLIP_NONE, getMenuEntryTexture(MenuEntry::PICTOCHAT));
			glSprite(129, iconYpos[2], GL_FLIP_NONE, getMenuEntryTexture(MenuEntry::DOWNLOADPLAY));
			glSprite(33, iconYpos[3], GL_FLIP_NONE, getMenuEntryTexture(MenuEntry::GBA));
			int num = (io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA) ? 1 : 0;
			if (num == 0 && ms().gbaBooter == TWLSettings::EGbaNativeGbar2) {
				glSprite(40, iconYpos[3]+6, GL_FLIP_NONE, &gbaIconImage[(((u8*)GBAROM)[0xB2] == 0x96) ? 0 : 1]);
			}
			else drawIcon(num, 40, iconYpos[3]+6);
			if (sys().isRegularDS() || (dsiFeatures() && ms().consoleModel < 2)) {
				glSprite(10, iconYpos[4], GL_FLIP_NONE, getMenuEntryTexture(MenuEntry::BRIGHTNESS));
			}
			if (bnrWirelessIcon[num] > 0) glSprite(207, iconYpos[3]+30, GL_FLIP_NONE, &wirelessIcons[(bnrWirelessIcon[1]-1) & 31]);
			// Playback animated icon
			if (bnriconisDSi[1]==true) {
				playBannerSequence(1);
			}
			if (!ms().kioskMode) {
				glSprite(117, iconYpos[5], GL_FLIP_NONE, getMenuEntryTexture(MenuEntry::SETTINGS));
			}
			glSprite(235, iconYpos[6], GL_FLIP_NONE, getMenuEntryTexture(MenuEntry::MANUAL));

			// Draw cursor
			if (showCursor) {
				auto drawCursorRect = [](int x1, int y1, int x2, int y2) {
						glSprite(x1, y1, GL_FLIP_NONE, &cursorImage[0]);
						glSprite(x2, y1, GL_FLIP_NONE, &cursorImage[1]);
						glSprite(x1, y2, GL_FLIP_NONE, &cursorImage[2]);
						glSprite(x2, y2, GL_FLIP_NONE, &cursorImage[3]);
				};
				
				updateCursorTargetPos();
				
				drawCursorRect(std::roundf(cursorTL), std::roundf(cursorBL), std::roundf(cursorTR), std::roundf(cursorBR));
			}
		}
		/*if (showdialogbox) {
			glBoxFilled(15, 79, 241, 129+(dialogboxHeight*8), RGB15(0, 0, 0));
			glBoxFilledGradient(16, 80, 240, 94, RGB15(0, 0, 31), RGB15(0, 0, 15), RGB15(0, 0, 15), RGB15(0, 0, 31));
			glBoxFilled(16, 96, 240, 128+(dialogboxHeight*8), RGB15(31, 31, 31));
		}*/
	  }
	  glEnd2D();
	  GFX_FLUSH = 0;

		frameDelay = 0;
		frameDelayEven = !frameDelayEven;
		renderFrame = false;
	}

	if (!whiteScreen) {
		for (int i = 0; i < 7; i++) {
			if (moveIconUp[i]) {
				iconYpos[i] -= 6;
			}
		}
	}
}

static void clockNeedleDraw(int angle, u32 length, u16 color) {
	if (ms().macroMode) return;
	
	constexpr float PI = 3.1415926535897f;
	
	float radians = (float)(angle%360) * (PI / 180.0f);

	// used for the calculations
	float x = clockXPos + 48.0f; 
	float y = clockYPos + 48.0f; 
	
	// used for drawing
	int xx, yy;

	float cos = std::cos(radians);
	float sin = std::sin(radians);

	for (u32 i = 0; i < length; i++) {
		x += cos;
		y -= sin;

		xx = x;
		yy = y;

		BG_GFX_SUB[yy*256+xx] = color;
		BG_GFX_SUB[(yy+1)*256+xx] = color;
		BG_GFX_SUB[yy*256+(xx-1)] = color;
		BG_GFX_SUB[(yy+1)*256+(xx-1)] = color;
	}
}

static void markerLoad(void) {
	char filePath[256];
	snprintf(filePath, sizeof(filePath), "nitro:/graphics/calendar/marker/%i.png", getFavoriteColor());
	FILE* file = fopen(filePath, "rb");

	if (file) {
		// Start loading
		std::vector<unsigned char> image;
		unsigned width, height;
		lodepng::decode(image, width, height, filePath);
		for (unsigned i=0;i<image.size()/4;i++) {
			markerImageBuffer[i] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
			if (colorTable) {
				markerImageBuffer[i] = colorTable[markerImageBuffer[i]];
			}
		}
	}

	fclose(file);
}

static void markerDraw(int x, int y) {
	u16* src = markerImageBuffer;

	int dstX;
	for (int yy = 0; yy < 13; yy++) {
		dstX = x;
		for (int xx = 0; xx < 13; xx++) {
			BG_GFX_SUB[y*256+dstX] = *src;
			dstX++;
			src++;
		}
		y++;
	}
}

static void calendarTextDraw(const Datetime& now) {
	printTopSmall(calendarXPos+56, calendarYPos+3, getDateYear());

	Datetime firstDay(now.getYear(), now.getMonth(), 1);
	int startWeekday = firstDay.getWeekDay();
	
	// Draw marker
	{
		int myPos = (startWeekday + now.getDay() - 1) / 7;
		markerDraw(calendarXPos+now.getWeekDay()*16+2, calendarYPos+myPos*16+34);
	}

	// Draw dates
	{
		int date = 1;
		int end = startWeekday+firstDay.getMonthDays();
		for (int i = startWeekday; i < end; ++i) {
			int cxPos = i % 7;
			int cyPos = i / 7;

			printTopSmall(calendarXPos+cxPos*16+9, calendarYPos+cyPos*16+35, std::to_string(date));

			date++;
		}
	}
}

void calendarDraw() {
	if (ms().macroMode) return;

	int calendarHeight = 113;
	u16* src = calendarImageBuffer;

	Datetime datetime = Datetime::now();
	Datetime firstDay(datetime.getYear(), datetime.getMonth(), 1);

	// If the dates exceed the small calendar then use the big calendar
	if (firstDay.getWeekDay() + firstDay.getMonthDays() > 7*5) {
		calendarHeight = 129;
		src = calendarBigImageBuffer;
	}

	int xDst = calendarXPos;
	int yDst = calendarYPos;
	for (int yy = 0; yy < 129; yy++) {
		xDst = calendarXPos;
		for (int xx = 0; xx < 113; xx++) {
			if (yy < calendarHeight)
				BG_GFX_SUB[yDst*256+xDst] = *(src++);
			else
				BG_GFX_SUB[yDst*256+xDst] = topImageBuffer[yDst*256+xDst]; // clear bottom for the small calendar

			xDst++;
		}
		yDst++;
	}

	calendarTextDraw(datetime);
}

void calendarLoad(void) {
	if (ms().macroMode) return;

	markerLoad();

	// Small calendar

	const char* filePath = "nitro:/graphics/calendar/calendar.png";
	FILE* file = fopen(filePath, "rb");

	if (file) {
		// Start loading
		std::vector<unsigned char> image;
		unsigned width, height;
		lodepng::decode(image, width, height, filePath);
		for (unsigned i=0;i<image.size()/4;i++) {
			calendarImageBuffer[i] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
			if (colorTable) {
				calendarImageBuffer[i] = colorTable[calendarImageBuffer[i]];
			}
		}
	}

	fclose(file);
	
	// Big calendar

	filePath = "nitro:/graphics/calendar/calendarbig.png";
	file = fopen(filePath, "rb");

	if (file) {
		// Start loading
		std::vector<unsigned char> image;
		unsigned width, height;
		lodepng::decode(image, width, height, filePath);
		for (unsigned i=0;i<image.size()/4;i++) {
			calendarBigImageBuffer[i] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
			if (colorTable) {
				calendarBigImageBuffer[i] = colorTable[calendarBigImageBuffer[i]];
			}
		}
	}

	fclose(file);

	calendarDraw();
}

void clockLoad(void) {
	if (ms().macroMode) return;

	const char* filePath = "nitro:/graphics/clock.png";
	FILE* file = fopen(filePath, "rb");

	if (file) {
		// Start loading
		std::vector<unsigned char> image;
		unsigned width, height;
		lodepng::decode(image, width, height, filePath);
		for (unsigned i=0;i<image.size()/4;i++) {
			clockImageBuffer[i] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
			if (colorTable) {
				clockImageBuffer[i] = colorTable[clockImageBuffer[i]];
			}
		}

		clockDraw();
	}
	fclose(file);
}

void clockDraw() {
	if (ms().macroMode) return;

	Datetime time = Datetime::now();

	u16* src = clockImageBuffer;
		
	int dstX = clockXPos;
	int dstY = clockYPos;
	for (int yy = 0; yy < 97; yy++) {
		dstX = clockXPos;
		for (int xx = 0; xx < 97; xx++) {
			BG_GFX_SUB[dstY*256+dstX] = *(src++);
			dstX++;
		}
		dstY++;
	}
	
	float h = (float)time.getHour() + (float)time.getMinute() / 60.0f;

	clockNeedleDraw(90-(h * 30), 24, clockNeedleColor); // hour
	clockNeedleDraw(90-(time.getMinute() * 6), 32, clockNeedleColor); // minute
	clockNeedleDraw(90-(time.getSecond() * 6), 36, userColors[getFavoriteColor()]); // second

	// draw clock pin
	for (int yy = clockYPos+46; yy < clockYPos+46+5; yy++) {
		for (int xx = clockXPos+46; xx < clockXPos+46+5; xx++) {
			BG_GFX_SUB[yy*256+xx] = clockPinColor;
		}
	}
}

// No longer used.
/*void loadBoxArt(const char* filename, bool secondaryDevice) {
	if (ms().macroMode) return;

	if (access(filename, F_OK) != 0) {
		switch (boxArtType[secondaryDevice]) {
			case -1:
				return;
			case 0:
			default:
				filename = "nitro:/graphics/boxart_unknown.png";
				break;
			case 1:
				filename = "nitro:/graphics/boxart_unknown1.png";
				break;
			case 2:
				filename = "nitro:/graphics/boxart_unknown2.png";
				break;
			case 3:
				filename = "nitro:/graphics/boxart_unknown3.png";
				break;
		}
	}

	std::vector<unsigned char> image;
	uint imageXpos, imageYpos, imageWidth, imageHeight;
	lodepng::decode(image, imageWidth, imageHeight, filename);
	if (imageWidth > 256 || imageHeight > 192)	return;

	imageXpos = (256-imageWidth)/2;
	imageYpos = (192-imageHeight)/2;

	int photoXstart = imageXpos;
	int photoXend = imageXpos+imageWidth;
	int photoX = photoXstart;
	int photoY = imageYpos;

	for (uint i=0;i<image.size()/4;i++) {
		u16 color = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
		if (colorTable) {
			color = colorTable[color];
		}
		if (image[(i*4)+3] == 0) {
			bmpImageBuffer[i] = color;
		} else {
			bmpImageBuffer[i] = alphablend(color, topImageBuffer[(photoY*256)+photoX], image[(i*4)+3]);
		}
		photoX++;
		if (photoX == photoXend) {
			photoX = photoXstart;
			photoY++;
		}
	}

	// Re-load top BG (excluding top bar)
	u16* src = topImageBuffer+(256*16);
	int x = 0;
	int y = 16;
	for (int i=256*16; i<256*192; i++) {
		if (x >= 256) {
			x = 0;
			y++;
		}
		BG_GFX_SUB[y*256+x] = *(src++);
		x++;
	}

	src = bmpImageBuffer;
	for (uint y = 0; y < imageHeight; y++) {
		for (uint x = 0; x < imageWidth; x++) {
			BG_GFX_SUB[(y+imageYpos) * 256 + imageXpos + x] = *(src++);
		}
	}
}*/

void topBgLoad(void) {
	if (ms().macroMode) return;

	char filePath[256];
	sprintf(filePath, "nitro:/graphics/%s.png", sys().isDSPhat() ? "phat_topbg" : "topbg");

	char temp[256];

	switch (ms().theme) {
		case TWLSettings::EThemeDSi: // DSi Theme
			sprintf(temp, "%s:/_nds/TwilightMenu/dsimenu/themes/%s/quickmenu/%s.png", sdFound() ? "sd" : "fat", ms().dsi_theme.c_str(), "topbg");
			break;
		case TWLSettings::ETheme3DS:
			sprintf(temp, "%s:/_nds/TwilightMenu/3dsmenu/themes/%s/quickmenu/%s.png", sdFound() ? "sd" : "fat", ms()._3ds_theme.c_str(), "topbg");
			break;
		case TWLSettings::EThemeR4:
			sprintf(temp, "%s:/_nds/TwilightMenu/r4menu/themes/%s/quickmenu/%s.png", sdFound() ? "sd" : "fat", ms().r4_theme.c_str(), "topbg");
			break;
		case TWLSettings::EThemeWood:
			// sprintf(temp, "%s:/_nds/TwilightMenu/akmenu/themes/%s/quickmenu/%s.png", sdFound() ? "sd" : "fat", ms().ak_theme.c_str(), "topbg");
			break;
		case TWLSettings::EThemeSaturn:
			sprintf(temp, "nitro:/graphics/%s.png", "topbg_saturn");
			break;
		case TWLSettings::EThemeHBL:
		case TWLSettings::EThemeGBC:
			break;
	}

	if (access(temp, F_OK) == 0)
		sprintf(filePath, "%s", temp);

	std::vector<unsigned char> image;
	uint imageWidth, imageHeight;
	lodepng::decode(image, imageWidth, imageHeight, std::string(filePath));
	if (imageWidth > 256 || imageHeight > 192)	return;

	for (uint i=0;i<image.size()/4;i++) {
		topImageBuffer[i] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
		if (colorTable) {
			topImageBuffer[i] = colorTable[topImageBuffer[i]];
		}
	}

	// Start loading
	u16* src = topImageBuffer;
	int x = 0;
	int y = 0;
	for (int i=0; i<256*192; i++) {
		if (x >= 256) {
			x = 0;
			y++;
		}
		BG_GFX_SUB[y*256+x] = *(src++);
		x++;
	}
}

void topBarLoad(void) {
	if (ms().macroMode) return;

	char filePath[256];
	snprintf(filePath, sizeof(filePath), "nitro:/graphics/%s/%i.png", "topbar", getFavoriteColor());
	FILE* file = fopen(filePath, "rb");

	if (file) {
		// Start loading
		std::vector<unsigned char> image;
		unsigned width, height;
		lodepng::decode(image, width, height, filePath);
		for (unsigned i=0;i<image.size()/4;i++) {
			bmpImageBuffer[i] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
			if (colorTable) {
				bmpImageBuffer[i] = colorTable[bmpImageBuffer[i]];
			}
		}
		u16* src = bmpImageBuffer;
		int x = 0;
		int y = 0;
		for (int i = 0; i < 256 * 16; i++) {
			if (x >= 256) {
				x = 0;
				y++;
			}
			BG_GFX_SUB[y*256+x] = *(src++);
			x++;
		}
	}

	fclose(file);
}

void graphicsInit()
{
	*(u16*)(0x0400006C) |= BIT(14);
	*(u16*)(0x0400006C) &= BIT(15);
	SetBrightness(0, 31);
	SetBrightness(1, 31);

	if (ms().colorMode != "Default") {
		char colorTablePath[256];
		sprintf(colorTablePath, "%s:/_nds/colorLut/%s.lut", (sys().isRunFromSD() ? "sd" : "fat"), ms().colorMode.c_str());

		if (getFileSize(colorTablePath) == 0x20000) {
			colorTable = new u16[0x20000/sizeof(u16)];

			FILE* file = fopen(colorTablePath, "rb");
			fread(colorTable, 1, 0x20000, file);
			fclose(file);
		}
	}

	////////////////////////////////////////////////////////////
	videoSetMode(MODE_5_3D);
	videoSetModeSub(MODE_5_2D);

	// Initialize gl2d
	glScreen2D();
	// Make gl2d render on transparent stage.
	glClearColor(31,31,31,0);
	glDisable(GL_CLEAR_BMP);

	// Clear the GL texture state
	glResetTextures();

	// Set up enough texture memory for our textures
	// Bank A is just 128kb and we are using 194 kb of
	// sprites
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankB(VRAM_B_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankD(VRAM_D_TEXTURE);
	vramSetBankE(VRAM_E_TEX_PALETTE);
	vramSetBankF(VRAM_F_TEX_PALETTE_SLOT4);
	vramSetBankG(VRAM_G_TEX_PALETTE_SLOT5); // 16Kb of palette ram, and font textures take up 8*16 bytes.
	vramSetBankH(VRAM_H_SUB_BG_EXT_PALETTE);
	vramSetBankI(VRAM_I_SUB_SPRITE_EXT_PALETTE);

	lcdMainOnBottom();
	
	int bg3Main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	bgSetPriority(bg3Main, 3);

	int bg2Main = bgInit(2, BgType_Bmp8, BgSize_B8_256x256, 7, 0);
	bgSetPriority(bg2Main, 0);

	int bg3Sub = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	bgSetPriority(bg3Sub, 3);

	// int bg2Sub = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 3, 0);
	// bgSetPriority(bg2Sub, 0);

	bgSetPriority(0, 1); // Set 3D to below text

	/*if (widescreenEffects) {
		// Add black bars to left and right sides
		s16 c = cosLerp(0) >> 4;
		REG_BG3PA_SUB = ( c * 315)>>8;
		REG_BG3X_SUB = -29 << 8;
	}*/

	swiWaitForVBlank();

	u16* newPalette = (u16*)cursorPals+(getFavoriteColor()*16);
	if (colorTable) {
		for (int i2 = 0; i2 < 3; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	cursorTexID = glLoadTileSet(cursorImage, // pointer to glImage array
							32, // sprite width
							32, // sprite height
							32, // bitmap width
							128, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_128, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							3, // Length of the palette to use (3 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) cursorBitmap // image data generated by GRIT
							);

	newPalette = (u16*)iconboxPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 12; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	iconboxTexID = glLoadTileSet(iconboxImage, // pointer to glImage array
							256, // sprite width
							64, // sprite height
							256, // bitmap width
							128, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_256, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_128, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							12, // Length of the palette to use (12 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) iconboxBitmap // image data generated by GRIT
							);

	newPalette = (u16*)iconbox_pressedPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 12; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	iconboxPressedTexID = glLoadTileSet(iconboxPressedImage, // pointer to glImage array
							256, // sprite width
							64, // sprite height
							256, // bitmap width
							64, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_256, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							12, // Length of the palette to use (12 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) iconbox_pressedBitmap // image data generated by GRIT
							);

	newPalette = (u16*)wirelessiconsPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 16; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	wirelessiconTexID = glLoadTileSet(wirelessIcons, // pointer to glImage array
							32, // sprite width
							32, // sprite height
							32, // bitmap width
							64, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) wirelessiconsBitmap // image data generated by GRIT
							);

	newPalette = (u16*)pictodlpPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 12; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	pictodlpTexID = glLoadTileSet(pictodlpImage, // pointer to glImage array
							128, // sprite width
							64, // sprite height
							128, // bitmap width
							256, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_128, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_256, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							12, // Length of the palette to use (12 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) pictodlpBitmap // image data generated by GRIT
							);

	newPalette = (u16*)pictodlp_selectedPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 12; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	pictodlpSelectedTexID = glLoadTileSet(pictodlpSelectedImage, // pointer to glImage array
							128, // sprite width
							64, // sprite height
							128, // bitmap width
							128, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_128, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_128, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							12, // Length of the palette to use (12 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) pictodlp_selectedBitmap // image data generated by GRIT
							);

	newPalette = (u16*)icon_dscardPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 16; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	dscardiconTexID = glLoadTileSet(dscardIconImage, // pointer to glImage array
							32, // sprite width
							32, // sprite height
							32, // bitmap width
							64, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) icon_dscardBitmap // image data generated by GRIT
							);

	newPalette = (u16*)icon_gbamodePal;
	if (colorTable) {
		for (int i2 = 0; i2 < 16; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	gbaiconTexID = glLoadTileSet(gbaIconImage, // pointer to glImage array
							32, // sprite width
							32, // sprite height
							32, // bitmap width
							64, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) (icon_gbamodeBitmap) // image data generated by GRIT
							);

	newPalette = (u16*)cornericonsPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 16; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	cornericonTexID = glLoadTileSet(cornerIcons, // pointer to glImage array
							32, // sprite width
							32, // sprite height
							32, // bitmap width
							128, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_128, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) cornericonsBitmap // image data generated by GRIT
							);

	newPalette = (u16*)icon_settingsPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 16; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	settingsiconTexID = glLoadTileSet(settingsIconImage, // pointer to glImage array
							32, // sprite width
							32, // sprite height
							32, // bitmap width
							64, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_64, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) icon_settingsBitmap // image data generated by GRIT
							);

	newPalette = (u16*)icon_settings_awayPal;
	if (colorTable) {
		for (int i2 = 0; i2 < 16; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	newPalette = (u16*)userColors;
	if (colorTable) {
		for (int i2 = 0; i2 < 16; i2++) {
			*(newPalette+i2) = colorTable[*(newPalette+i2)];
		}
	}

	settingsiconAwayTexID = glLoadTileSet(settingsIconAwayImage, // pointer to glImage array
							32, // sprite width
							32, // sprite height
							32, // bitmap width
							32, // bitmap height
							GL_RGB16, // texture type for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeX for glTexImage2D() in videoGL.h
							TEXTURE_SIZE_32, // sizeY for glTexImage2D() in videoGL.h
							TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, // param for glTexImage2D() in videoGL.h
							16, // Length of the palette to use (16 colors)
							(u16*) newPalette, // Load our 16 color tiles palette
							(u8*) icon_settings_awayBitmap // image data generated by GRIT
							);

	loadConsoleIcons();

	updateCursorTargetPos();
	cursorTL = cursorTargetTL;
	cursorBL = cursorTargetBL;
	cursorTR = cursorTargetTR;
	cursorBR = cursorTargetBR;

	clockNeedleColor = CONVERT_COLOR(121,121,121);
	if (colorTable) {
		clockNeedleColor = colorTable[clockNeedleColor];
	}

	clockPinColor = CONVERT_COLOR(73, 73, 73);
	if (colorTable) {
		clockPinColor = colorTable[clockPinColor];
	}

	irqSet(IRQ_VBLANK, vBlankHandler);
	irqEnable(IRQ_VBLANK);
	irqSet(IRQ_VCOUNT, frameRateHandler);
	irqEnable(IRQ_VCOUNT);
}
