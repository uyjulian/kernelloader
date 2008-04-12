/* Copyright (c) 2007 Mega Man */
#include "gsKit.h"
#include "dmaKit.h"
#include "malloc.h"
#include "stdio.h"
#include "kernel.h"

#include "config.h"
#include "menu.h"
#include "rom.h"
#include "graphic.h"
#include "loader.h"
#include <screenshot.h>


/** Maximum buffer size for error_printf(). */
#define MAX_BUFFER 128

/** Maximum buffer size of info message buffer. */
#define MAX_INFO_BUFFER 4096

/** Maximum number of error messages. */
#define MAX_MESSAGES 10

/** True, if graphic is initialized. */
static bool graphicInitialized = false;

static Menu *menu = NULL;

static bool enableDisc = 0;

/** gsGlobal is required for all painting functiions of gsKit. */
static GSGLOBAL *gsGlobal = NULL;

/** Colours used for painting. */
static u64 White, Black, Blue, Red;

/** Text colour. */
static u64 TexCol;

/** Red text colour. */
static u64 TexRed;

/** Black text colour. */
static u64 TexBlack;

/** Font used for printing text. */
static GSFONT *gsFont;

/** File name that is printed on screen. */
static const char *loadName = NULL;

static const char *statusMessage = NULL;

/** Percentage for loading file shown as progress bar. */
static int loadPercentage = 0;

/** Scale factor for font. */
static float scale = 1.0f;

/** Ring buffer with error messages. */
static const char *errorMessage[MAX_MESSAGES] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

/** Read pointer into ring buffer of error messages. */
static int readMsgPos = 0;

/** Write pointer into ring buffer of error messages. */
static int writeMsgPos = 0;

static GSTEXTURE *texFolder = NULL;

static GSTEXTURE *texUp = NULL;

static GSTEXTURE *texBack = NULL;

static GSTEXTURE *texSelected = NULL;

static GSTEXTURE *texUnselected = NULL;

static GSTEXTURE *texPenguin = NULL;

static GSTEXTURE *texDisc = NULL;

static GSTEXTURE *texCloud = NULL;

static int reservedEndOfDisplayY = 42;

static bool usePad = false;

int scrollPos = 0;

int inputScrollPos = 0;

void paintTexture(GSTEXTURE *tex, int x, int y, int z)
{
	if (tex != NULL) {
		gsKit_prim_sprite_texture(gsGlobal, tex, x, y, 0, 0, x + tex->Width, y + tex->Height, tex->Width, tex->Height, z, 0x80808080 /* color */);
	}
}

static char infoBuffer[MAX_INFO_BUFFER];
static int infoBufferPos = 0;

static char *inputBuffer = NULL;

int printTextBlock(int x, int y, int z, int maxCharsPerLine, int maxY, const char *msg, int scrollPos)
{
	char lineBuffer[maxCharsPerLine];
	int i;
	int pos;
	int lastSpace;
	int lastSpacePos;
	int lineNo;

	pos = 0;
	lineNo = 0;
	do {
		i = 0;
		lastSpace = 0;
		lastSpacePos = 0;
		while (i < maxCharsPerLine) {
			lineBuffer[i] = msg[pos];
			if (msg[pos] == 0) {
				lastSpace = i;
				lastSpacePos = pos;
				i++;
				break;
			}
			if (msg[pos] == ' ') {
				lastSpace = i;
				lastSpacePos = pos + 1;
				if (i != 0) {
					i++;
				}
			} else if (msg[pos] == '\r') {
				lineBuffer[i] = 0;
				lastSpace = i;
				lastSpacePos = pos + 1;
			} else if (msg[pos] == '\n') {
				lineBuffer[i] = 0;
				lastSpace = i;
				lastSpacePos = pos + 1;
				pos++;
				break;
			} else {
				i++;
			}
			pos++;
		}
		if (lastSpace != 0) {
			pos = lastSpacePos;
		} else {
			lastSpace = i;
		}
		lineBuffer[lastSpace] = 0;

		if (lineNo >= scrollPos) {
#if 0
			printf("Test pos %d i %d lastSpacePos %d %s\n", pos, i, lastSpacePos, lineBuffer);
#else
			gsKit_font_print_scaled(gsGlobal, gsFont, x, y, z, scale, TexCol,
				lineBuffer);
#endif
			y += 30;
			if (y > (maxY - 30)) {
				break;
			}
		}
		lineNo++;
	} while(msg[pos] != 0);
	if (lineNo < scrollPos) {
		return lineNo;
	} else {
		return scrollPos;
	}
}

/** Paint current state on screen. */
void graphic_paint(void)
{
	const char *msg;

	if (!graphicInitialized) {
		return;
	}

	gsKit_clear(gsGlobal, Blue);

	/* Paint background. */
	paintTexture(texCloud, 0, 0, 0);

	paintTexture(texPenguin, 5, 10, 1);

	gsKit_font_print_scaled(gsGlobal, gsFont, 110, 50, 3, scale, TexCol,
		"Loader for Linux " LOADER_VERSION);
	gsKit_font_print_scaled(gsGlobal, gsFont, 490, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.5, TexBlack,
		"by Mega Man");
	gsKit_font_print_scaled(gsGlobal, gsFont, 490, gsGlobal->Height - reservedEndOfDisplayY + 15, 3, 0.5, TexBlack,
		"04.02.2008");

	if (statusMessage != NULL) {
		gsKit_font_print_scaled(gsGlobal, gsFont, 50, 90, 3, scale, TexCol,
			statusMessage);
	} else if (loadName != NULL) {
		gsKit_font_print_scaled(gsGlobal, gsFont, 50, 90, 3, scale, TexCol,
			loadName);
		gsKit_prim_sprite(gsGlobal, 50, 120, 50 + 520, 140, 2, White);
		if (loadPercentage > 0) {
			gsKit_prim_sprite(gsGlobal, 50, 120,
				50 + (520 * loadPercentage) / 100, 140, 2, Red);
		}
	}
	msg = getErrorMessage();
	if (msg != NULL) {
		gsKit_font_print_scaled(gsGlobal, gsFont, 50, 170, 3, scale, TexRed,
			"Error Message:");
		printTextBlock(50, 230, 3, 26, gsGlobal->Height - reservedEndOfDisplayY, msg, 0);
	} else {
		if (!isInfoBufferEmpty()) {
			scrollPos = printTextBlock(50, 170, 3, 26, gsGlobal->Height - reservedEndOfDisplayY, infoBuffer, scrollPos);
		} else {
			if (inputBuffer != NULL) {
				inputScrollPos = printTextBlock(50, 170, 3, 26, gsGlobal->Height - reservedEndOfDisplayY, inputBuffer, inputScrollPos);
			} else if (menu != NULL) {
				menu->paint();
			}
		}
	}
	if (enableDisc) {
		paintTexture(texDisc, 100, 200, 100);
		gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
			"Loading, please wait...");
	} else {
		if (msg != NULL) {
			if (usePad) {
				gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
					"Press CROSS to continue.");
			}
		} else {
			if (!isInfoBufferEmpty()) {
				if (usePad) {
					gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
						"Press CROSS to continue.");
					gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY + 18, 3, 0.8, TexBlack,
						"Use UP and DOWN to scroll.");
				}
			} else {
				if (inputBuffer != NULL) {
					gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
						"Please use USB keyboard.");
					gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY + 18, 3, 0.8, TexBlack,
						"Press CROSS to quit.");
				} else if (menu != NULL) {
					if (usePad) {
						gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
							"Press CROSS to select menu.");
						gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY + 18, 3, 0.8, TexBlack,
							"Use UP and DOWN to scroll.");
					}
				}
			}
		}
		if (!usePad) {
			gsKit_font_print_scaled(gsGlobal, gsFont, 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
				"Please wait...");
		}
	}
	gsKit_queue_exec(gsGlobal);
	gsKit_sync_flip(gsGlobal);
}

extern "C" {

	/**
	 * Set load percentage of file.
	 * @param percentage Percentage to set (0 - 100).
	 * @param name File name printed on screen.
	 */
	void graphic_setPercentage(int percentage, const char *name) {
		if (percentage > 100) {
			percentage = 100;
		}
		loadPercentage = percentage;

		loadName = name;
		graphic_paint();
	}

	/**
	 * Set status message.
	 * @param text Text displayed on screen.
	 */
	void graphic_setStatusMessage(const char *text) {
		statusMessage = text;
		graphic_paint();
	}
}

GSTEXTURE *getTexture(const char *filename)
{
	GSTEXTURE *tex = NULL;
	rom_entry_t *romfile;
	romfile = rom_getFile(filename);
	if (romfile != NULL) {
		tex = (GSTEXTURE *) malloc(sizeof(GSTEXTURE));
		if (tex != NULL) {
			tex->Width = romfile->width;
			tex->Height = romfile->height;
			if (romfile->depth == 4) {
				tex->PSM = GS_PSM_CT32;
			} else {
				tex->PSM = GS_PSM_CT24;
			}
			tex->Mem = (u32 *) romfile->start;
			tex->Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(tex->Width, tex->Height, tex->PSM), GSKIT_ALLOC_USERBUFFER);
			tex->Filter = GS_FILTER_LINEAR;
			if (tex->Vram != GSKIT_ALLOC_ERROR) {
				gsKit_texture_upload(gsGlobal, tex);
			} else {
				printf("Out of VRAM \"%s\".\n", filename);
				free(tex);
				error_printf("Out of VRAM while loading texture (%s).", filename);
				return NULL;
			}
		} else {
			error_printf("Out of memory while loading texture (%s).", filename);
		}
	} else {
		error_printf("Failed to open texture \"%s\".", filename);
	}
	return tex;
}

/**
 * Initialize graphic screen.
 * @param mode Graphic mode to use.
 */
Menu *graphic_main(graphic_mode_t mode)
{
	int i;
	int numberOfMenuItems;

	//gsGlobal = gsKit_init_global(GS_MODE_AUTO_I);
	if (mode == MODE_NTSC) {
		gsGlobal = gsKit_init_global(GS_MODE_NTSC_I);
		numberOfMenuItems = 7;
	} else {
		gsGlobal = gsKit_init_global(GS_MODE_PAL_I);
		numberOfMenuItems = 8;
	}
	//  GSGLOBAL *gsGlobal = gsKit_init_global(GS_MODE_VGA_640_60);


	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
		D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	// Initialize the DMAC
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_FROMSPR);
	dmaKit_chan_init(DMA_CHANNEL_TOSPR);

	Black = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00);
	White = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x00, 0x00);
	Blue = GS_SETREG_RGBAQ(0x10, 0x10, 0xF0, 0x00, 0x00);
	Red = GS_SETREG_RGBAQ(0xF0, 0x10, 0x10, 0x00, 0x00);

	TexCol = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00);
	TexRed = GS_SETREG_RGBAQ(0xF0, 0x10, 0x10, 0x80, 0x00);
	TexBlack = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);

	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;

	gsKit_init_screen(gsGlobal);

	gsFont = gsKit_init_font(GSKIT_FTYPE_FONTM, NULL);
	if (gsKit_font_upload(gsGlobal, gsFont) != 0) {
		printf("Can't find any font to use\n");
		SleepThread();
	}

	gsFont->FontM_Spacing = 0.8f;
	texFolder = getTexture("folder.rgb");
	texUp = getTexture("up.rgb");
	texBack = getTexture("back.rgb");
	texSelected = getTexture("selected.rgb");
	texUnselected = getTexture("unselected.rgb");
	texPenguin = getTexture("penguin.rgb");
	texDisc = getTexture("disc.rgb");
	texCloud = getTexture("cloud.rgb");

	menu = new Menu(gsGlobal, gsFont, numberOfMenuItems);
	menu->setPosition(50, 120);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

	/* Activate graphic routines. */
	graphicInitialized = true;

	for (i = 0; i < 2; i++) {
		graphic_paint();
	}
	return menu;
}

int setCurrentMenu(void *arg)
{
	Menu *newMenu = (Menu *) arg;

	menu = newMenu;

	return 0;
}

Menu *getCurrentMenu(void)
{
	return menu;
}

GSTEXTURE *getTexFolder(void)
{
	return texFolder;
}

GSTEXTURE *getTexUp(void)
{
	return texUp;
}

GSTEXTURE *getTexBack(void)
{
	return texBack;
}

GSTEXTURE *getTexSelected(void)
{
	return texSelected;
}

GSTEXTURE *getTexUnselected(void)
{
	return texUnselected;
}

extern "C" {
	void setErrorMessage(const char *text) {
		if (errorMessage[writeMsgPos] == NULL) {
			errorMessage[writeMsgPos] = text;
			writeMsgPos = (writeMsgPos + 1) % MAX_MESSAGES;
		} else {
			printf("Error message queue is full at error:\n");
			printf("%s\n", text);
		}
	}

	void goToNextErrorMessage(void)
	{
		if (errorMessage[readMsgPos] != NULL) {
			errorMessage[readMsgPos] = NULL;
			readMsgPos = (readMsgPos + 1) % MAX_MESSAGES;
		}
	}

	const char *getErrorMessage(void) {
		return errorMessage[readMsgPos];
	}

	int error_printf(const char *format, ...)
	{
		static char buffer[MAX_MESSAGES][MAX_BUFFER];
		int ret;
		va_list varg;
		va_start(varg, format);

		if (errorMessage[writeMsgPos] == NULL) {
			ret = vsnprintf(buffer[writeMsgPos], MAX_BUFFER, format, varg);

			setErrorMessage(buffer[writeMsgPos]);

			if (graphicInitialized) {
				if (readMsgPos == writeMsgPos) {
					/* Show it before doing anything else. */
					graphic_paint();
				}
			}
		} else {
			printf("error_printf loosing message: %s\n", format);
			ret = -1;
		}

		va_end(varg);
		return ret;
	}

	void info_prints(const char *text)
	{
		int len = strlen(text) + 1;
		int remaining;

		if (len > MAX_INFO_BUFFER) {
			printf("info_prints(): text too long.\n");
			return;
		}

		remaining = MAX_INFO_BUFFER - infoBufferPos;
		if (len > remaining) {
			int required;
			int i;

			/* required space in buffer. */
			required = len - remaining;

			/* Find next new line. */
			for (i = required; i < MAX_INFO_BUFFER; i++) {
				if (infoBuffer[i] == '\n') {
					i++;
					break;
				}
			}

			if (i >= MAX_INFO_BUFFER) {
				/* Delete complete buffer, buffer doesn't have any carriage returns. */
				infoBufferPos = 0;
			} else {
				/* Scroll buffer and delete old stuff. */
				for (i = 0; i < (infoBufferPos - required); i++) {
					infoBuffer[i] = infoBuffer[required + i];
				}
				infoBufferPos = infoBufferPos - required;
			}
			infoBufferPos -= required;
		}
		strcpy(&infoBuffer[infoBufferPos], text);
		infoBufferPos += len - 1;
	}

	int info_printf(const char *format, ...)
	{
		int ret;
		static char buffer[MAX_BUFFER];
		va_list varg;
		va_start(varg, format);

		ret = vsnprintf(buffer, MAX_BUFFER, format, varg);
		info_prints(buffer);

		va_end(varg);
		return ret;
	}

	void setEnableDisc(int v)
	{
		enableDisc = v;

		/* Show it before doing anything else. */
		graphic_paint();
	}

	void scrollUpFast(void)
	{
		int i;

		for (i = 0; i < 8; i++) {
			scrollUp();
		}
	}

	void scrollUp(void)
	{
		if (inputBuffer != NULL) {
			inputScrollPos--;
			if (inputScrollPos < 0) {
				inputScrollPos = 0;
			}
		} else {
			scrollPos--;
			if (scrollPos < 0) {
				scrollPos = 0;
			}
		}
	}

	void scrollDownFast(void)
	{
		int i;

		for (i = 0; i < 8; i++) {
			scrollDown();
		}
	}

	void scrollDown(void)
	{
		if (inputBuffer != NULL) {
			inputScrollPos++;
		} else {
			scrollPos++;
		}
	}

	int getScrollPos(void)
	{
		return scrollPos;
	}

	int isInfoBufferEmpty(void)
	{
		return !(loaderConfig.enableEEDebug && (infoBufferPos > 0));
	}

	void clearInfoBuffer(void)
	{
		infoBuffer[0] = 0;
		scrollPos = 0;
		infoBufferPos = 0;

		if (graphicInitialized) {
			/* Show it before doing anything else. */
			graphic_paint();
		}
	}
	void enablePad(int val)
	{
		usePad = val;
	}

	void setInputBuffer(char *buffer)
	{
		inputBuffer = buffer;
	}

	char *getInputBuffer(void)
	{
		return inputBuffer;
	}

	void graphic_screenshot(void)
	{
		static int screenshotCounter = 0;
		char text[256];

#ifdef RESET_IOP
		snprintf(text, 256, "mass0:kloader%d.tga", screenshotCounter);
#else
		snprintf(text, 256, "host:kloader%d.tga", screenshotCounter);
#endif
		ps2_screenshot_file(text, gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1],
			gsGlobal->Width, gsGlobal->Height / 2, gsGlobal->PSM);
		screenshotCounter++;
	}
}

