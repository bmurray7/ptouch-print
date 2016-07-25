/*
	ptouch-print - Print labels with images or text on a Brother P-Touch
	
	Copyright (C) 2015 Dominic Radermacher <dominic.radermacher@gmail.com>

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License version 3 as
	published by the Free Software Foundation
	
	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software Foundation,
	Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>	/* printf() */
#include <stdlib.h>	/* exit(), malloc() */
#include <string.h>	/* strcmp(), memcmp() */
#include <sys/types.h>	/* open() */
#include <sys/stat.h>	/* open() */
#include <fcntl.h>	/* open() */
#include <gd.h>
#include "config.h"
#include "gettext.h"	/* gettext(), ngettext() */
#include "ptouch.h"

#define _(s) gettext(s)

#define MAX_LINES 4	/* maybe this should depend on tape size */

gdImage *image_load(const char *file);
void rasterline_setpixel(uint8_t rasterline[16], int pixel);
int get_baselineoffset(char *text, char *font, int fsz);
int find_fontsize(int want_px, char *font, char *text);
int needed_width(char *text, char *font, int fsz);
int print_img(ptouch_dev ptdev, gdImage *im);
int write_png(gdImage *im, const char *file);
gdImage *render_text(char *font, char *line[], int lines, int tape_width);
void usage(char *progname);
int parse_args(int argc, char **argv);

// char *font_file="/usr/share/fonts/TTF/Ubuntu-M.ttf";
// char *font_file="Ubuntu:medium";
char *font_file="/usr/share/fonts/liberation/LiberationSansNarrow-Regular.ttf";
char *save_png=NULL;
int verbose=0;
int fontsize=0;

/* --------------------------------------------------------------------
   -------------------------------------------------------------------- */

void rasterline_setpixel(uint8_t rasterline[16], int pixel)
{
	rasterline[15-(pixel/8)] |= 1<<(pixel%8);
	return;
}

int print_img(ptouch_dev ptdev, gdImage *im)
{
	int d,i,k,offset,tape_width;
	uint8_t rasterline[16];

	tape_width=ptouch_getmaxwidth(ptdev);
	/* find out whether color 0 or color 1 is darker */
	d=(gdImageRed(im,1)+gdImageGreen(im,1)+gdImageBlue(im,1) < gdImageRed(im,0)+gdImageGreen(im,0)+gdImageBlue(im,0))?1:0;
	if (gdImageSY(im) > tape_width) {
		printf(_("image is too large (%ipx x %ipx)\n"), gdImageSX(im), gdImageSY(im));
		printf(_("maximum printing width for this tape is %ipx\n"), tape_width);
		return -1;
	}
	offset=64-(gdImageSY(im)/2);	/* always print centered  */
	if (ptouch_rasterstart(ptdev) != 0) {
		printf(_("ptouch_rasterstart() failed\n"));
		return -1;
	}
	for (k=0; k<gdImageSX(im); k+=1) {
		memset(rasterline, 0, sizeof(rasterline));
		for (i=0; i<gdImageSY(im); i+=1) {
			if (gdImageGetPixel(im, k, gdImageSY(im)-1-i) == d) {
				rasterline_setpixel(rasterline, offset+i);
			}
		}
		if (ptouch_sendraster(ptdev, rasterline, 16) != 0) {
			printf(_("ptouch_send() failed\n"));
			return -1;
		}
	}
	return 0;
}

/* --------------------------------------------------------------------
	Function	image_load()
	Description	detect the type of a image and try to load it
	Last update	2005-10-16
	Status		Working, should add debug info
   -------------------------------------------------------------------- */

gdImage *image_load(const char *file)
{
	const uint8_t png[8]={0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a};
	char d[10];
	FILE *f;
	gdImage *img=NULL;

	if ((f = fopen(file, "rb")) == NULL) {	/* error cant open file */
		return NULL;
	}
	if (fread(d, sizeof(d), 1, f) != 1) {
		return NULL;
	}
	rewind(f);
	if (memcmp(d, png, 8) == 0) {
		img=gdImageCreateFromPng(f);
	}
	fclose(f);
	return img;
}

int write_png(gdImage *im, const char *file)
{
	FILE *f;

	if ((f = fopen(file, "wb")) == NULL) {
		printf(_("writing image '%s' failed\n"), file);
		return -1;
	}
	gdImagePng(im, f);
	fclose(f);
	return 0;
}

/* --------------------------------------------------------------------
	Find out the difference in pixels between a "normal" char and one
	that goes below the font baseline
   -------------------------------------------------------------------- */
int get_baselineoffset(char *text, char *font, int fsz)
{
	int brect[8];

	if (strpbrk(text, "QgjpqyQ") == NULL) {	/* if we have none of these */
		return 0;		/* we don't need an baseline offset */
	}				/* else we need to calculate it */
	gdImageStringFT(NULL, &brect[0], -1, font, fsz, 0.0, 0, 0, "o");
	int tmp=brect[1]-brect[5];
	gdImageStringFT(NULL, &brect[0], -1, font, fsz, 0.0, 0, 0, "g");
	return (brect[1]-brect[5])-tmp;
}

/* --------------------------------------------------------------------
	Find out which fontsize we need for a given font to get a
	specified pixel size
   -------------------------------------------------------------------- */
int find_fontsize(int want_px, char *font, char *text)
{
	int save=0;
	int brect[8];

	for (int i=4; ; i++) {
		if (gdImageStringFT(NULL, &brect[0], -1, font, i, 0.0, 0, 0, text) != NULL) {
			break;
		}
		if (brect[1]-brect[5] <= want_px) {
			save=i;
		} else {
			break;
		}
	}
	if (save == 0) {
		return -1;
	}
	return save;
}

int needed_width(char *text, char *font, int fsz)
{
	int brect[8];

	if (gdImageStringFT(NULL, &brect[0], -1, font, fsz, 0.0, 0, 0, text) != NULL) {
		return -1;
	}
	return brect[2]-brect[0];
}

gdImage *render_text(char *font, char *line[], int lines, int tape_width)
{
	int brect[8];
	int i, black, x=0, tmp, fsz=0, ofs;
	char *p;
	gdImage *im=NULL;

//	printf(_("%i lines, font = '%s'\n"), lines, font);
	if (gdFTUseFontConfig(1) != GD_TRUE) {
		printf(_("warning: font config not available\n"));
	}
	if (fontsize > 0) {
		fsz=fontsize;
		printf(_("setting font size=%i\n"), fsz);
	} else {
		for (i=0; i<lines; i++) {
			if ((tmp=find_fontsize(tape_width/lines, font, line[i])) < 0) {
				printf(_("could not estimate needed font size\n"));
				return NULL;
			}
			if ((fsz == 0) || (tmp < fsz)) {
				fsz=tmp;
			}
		}
		printf(_("choosing font size=%i\n"), fsz);
	}
	for(i=0; i<lines; i++) {
		tmp=needed_width(line[i], font_file, fsz);
		if (tmp > x) {
			x=tmp;
		}
	}
	im=gdImageCreatePalette(x, tape_width);
	gdImageColorAllocate(im, 255, 255, 255);
	black=gdImageColorAllocate(im, 0, 0, 0);
	/* gdImageStringFT(im,brect,fg,fontlist,size,angle,x,y,string) */
	for (i=0; i<lines; i++) {
		if ((p=gdImageStringFT(NULL, &brect[0], -black, font, fsz, 0.0, 0, 0, line[i])) != NULL) {
			printf(_("error in gdImageStringFT: %s\n"), p);
		}
		tmp=brect[1]-brect[5];
		ofs=get_baselineoffset(line[i], font_file, fsz);
//		printf("line %i height = %ipx, pos = %i\n", i+1, tmp, i*(tape_width/lines)+tmp-ofs-1);
		if ((p=gdImageStringFT(im, &brect[0], -black, font, fsz, 0.0, 0, i*(tape_width/lines)+tmp-ofs-1, line[i])) != NULL) {
			printf(_("error in gdImageStringFT: %s\n"), p);
		}
	}
	return im;
}

void usage(char *progname)
{
	printf("usage: %s [options] <print-command(s)>\n", progname);
	printf("options:\n");
	printf("\t--font <file>\t\tuse font <file> or <name>\n");
	printf("\t--writepng <file>\tinstead of printing, write output to png file\n");
	printf("\t\t\t\tThis currently works only when using\n\t\t\t\tEXACTLY ONE --text statement\n");
	printf("print-commands:\n");
	printf("\t--image <file>\t\tprint the given image which must be a 2 color\n");
	printf("\t\t\t\t(black/white) png\n");
	printf("\t--text <text>\t\tPrint 1-4 lines of text.\n");
	printf("\t\t\t\tIf the text contains spaces, use quotation marks\n\t\t\t\taround it.\n");
	printf("\t--cutmark\t\tPrint a mark where the tape should be cut\n");
	exit(1);
}

/* here we don't print anything, but just try to catch syntax errors */
int parse_args(int argc, char **argv)
{
	int lines, i;

	for (i=1; i<argc; i++) {
		if (*argv[i] != '-') {
			break;
		}
		if (strcmp(&argv[i][1], "-font") == 0) {
			if (i+1<argc) {
				font_file=argv[++i];
			} else {
				usage(argv[0]);
			}
		} else if (strcmp(&argv[i][1], "-fontsize") == 0) {
			if (i+1<argc) {
				i++;
			} else {
				usage(argv[0]);
			}
		} else if (strcmp(&argv[i][1], "-writepng") == 0) {
			if (i+1<argc) {
				save_png=argv[++i];
			} else {
				usage(argv[0]);
			}
		} else if (strcmp(&argv[i][1], "-cutmark") == 0) {
			continue;	/* not done here */
		} else if (strcmp(&argv[i][1], "-info") == 0) {
			continue;	/* not done here */
		} else if (strcmp(&argv[i][1], "-image") == 0) {
			if (i+1<argc) {
				i++;
			} else {
				usage(argv[0]);
			}
		} else if (strcmp(&argv[i][1], "-text") == 0) {
			for (lines=0; (lines < MAX_LINES) && (i < argc); lines++) {
				if ((i+1 >= argc) || (argv[i+1][0] == '-')) {
					break;
				}
				i++;
			}
		} else if (strcmp(&argv[i][1], "-version") == 0) {
			printf(_("ptouch-print version %s by Dominic Radermacher\n"), VERSION);
			exit(0);
		} else {
			usage(argv[0]);
		}
	}
	return i;
}

int main(int argc, char *argv[])
{
	int i, lines, tape_width;
	char *line[MAX_LINES];
	gdImage *im=NULL;
	ptouch_dev ptdev=NULL;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	i=parse_args(argc, argv);
	if (i != argc) {
		usage(argv[0]);
	}
	if ((ptouch_open(&ptdev)) < 0) {
		return 5;
	}
	if (ptouch_init(ptdev) != 0) {
		printf(_("ptouch_init() failed\n"));
	}
	if (ptouch_getstatus(ptdev) != 0) {
		printf(_("ptouch_getstatus() failed\n"));
		return 1;
	}
	tape_width=ptouch_getmaxwidth(ptdev);
	for (i=1; i<argc; i++) {
		if (*argv[i] != '-') {
			break;
		}
		if (strcmp(&argv[i][1], "-font") == 0) {
			if (i+1<argc) {
				font_file=argv[++i];
			} else {
				usage(argv[0]);
			}
		} else if (strcmp(&argv[i][1], "-fontsize") == 0) {
			if (i+1<argc) {
				fontsize=strtol(argv[++i], NULL, 10);
			} else {
				usage(argv[0]);
			}
		} else if (strcmp(&argv[i][1], "-writepng") == 0) {
			if (i+1<argc) {
				save_png=argv[++i];
			} else {
				usage(argv[0]);
			}
		} else if (strcmp(&argv[i][1], "-info") == 0) {
			printf(_("maximum printing width for this tape is %ipx\n"), tape_width);
			exit(0);
		} else if (strcmp(&argv[i][1], "-image") == 0) {
			im=image_load(argv[++i]);
			if (im != NULL) {
				print_img(ptdev, im);
				gdImageDestroy(im);
			}
		} else if (strcmp(&argv[i][1], "-text") == 0) {
			for (lines=0; (lines < MAX_LINES) && (i < argc); lines++) {
				if ((i+1 >= argc) || (argv[i+1][0] == '-')) {
					break;
				}
				i++;
				line[lines]=argv[i];
			}
			if ((im=render_text(font_file, line, lines, tape_width)) == NULL) {
				printf(_("could not render text\n"));
				return 1;
			}
			if (save_png != NULL) {
				write_png(im, save_png);
			} else {
				print_img(ptdev, im);
			}
			gdImageDestroy(im);
		} else if (strcmp(&argv[i][1], "-cutmark") == 0) {
			ptouch_cutmark(ptdev);
		} else {
			usage(argv[0]);
		}
	}
	if (ptouch_eject(ptdev) != 0) {
		printf(_("ptouch_eject() failed\n"));
		return -1;
	}
	ptouch_close(ptdev);
	libusb_exit(NULL);
	return 0;
}
