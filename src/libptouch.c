/*
	libptouch - functions to help accessing a brother ptouch

	Copyright (C) 2013 Dominic Radermacher <dominic.radermacher@gmail.com>

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

#define _POSIX_C_SOURCE	199309L	/* needed for nanosleep() when using -std=c11 */

#include <stdio.h>
#include <stdlib.h>	/* malloc() */
#include <string.h>	/* memcmp()  */
#include <sys/types.h>	/* open() */
#include <sys/stat.h>	/* open() */
#include <fcntl.h>	/* open() */
#include <time.h>	/* nanosleep(), struct timespec */
#include "config.h"
#include "gettext.h"	/* gettext(), ngettext() */
#include "ptouch.h"

#define _(s) gettext(s)

//#define DEBUG

struct _pt_tape_info tape_info[6]= {
	{6, 32},	/* 6mm tape is 32px wide? works for me ;-) */
	{9, 52},	/* 9mm tape is 52px wide? works for me ;-) */
	{12,76},	/* and 76px work for me on a 12mm tape - maybe its only 64px */
	{18,120},
	{24,128},
	{0,0}		/* terminating entry */
};

struct _pt_dev_info ptdevs[] = {
	{0x04f9, 0x202d, "PT-2430PC", 128, 0},	/* 180dpi, maximum 128px */
	{0x04f9, 0x202c, "PT-1230PC", 76, 0},	/* 180dpi, supports tapes up to 12mm - I don't know how much pixels it can print! */
	{0,0,"",0,0}
};

void ptouch_rawstatus(uint8_t raw[32]);

int ptouch_open(ptouch_dev *ptdev)
{
	libusb_device **devs;
	libusb_device *dev;
	libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor desc;
	ssize_t cnt;
	int r,i=0;

	if ((*ptdev=malloc(sizeof(struct _ptouch_dev))) == NULL) {
		fprintf(stderr, _("out of memory\n"));
		return -1;
	}
	if ((libusb_init(NULL)) < 0) {
		fprintf(stderr, _("libusb_init() failed\n"));
		return -1;
	}
#ifdef DEBUG
	libusb_set_debug(NULL, 3);
#endif /* DEBUG */
	if ((cnt=libusb_get_device_list(NULL, &devs)) < 0) {
		return -1;
	}
	while ((dev=devs[i++]) != NULL) {
		if ((r=libusb_get_device_descriptor(dev, &desc)) < 0) {
			fprintf(stderr, _("failed to get device descriptor"));
			libusb_free_device_list(devs, 1);
			return -1;
		}
		for (int k=0; ptdevs[k].vid > 0; k++) {
			if ((desc.idVendor == ptdevs[k].vid) && (desc.idProduct == ptdevs[k].pid) && (ptdevs[k].flags >= 0)) {
				fprintf(stderr, _("%s found on USB bus %d, device %d\n"),
					ptdevs[k].name,
					libusb_get_bus_number(dev),
					libusb_get_device_address(dev));
				if ((r=libusb_open(dev, &handle)) != 0) {
					fprintf(stderr, _("libusb_open error :%s\n"), libusb_error_name(r));
					return -1;
				}
				libusb_free_device_list(devs, 1);
				if ((r=libusb_kernel_driver_active(handle, 0)) == 1) {
					if ((r=libusb_detach_kernel_driver(handle, 0)) != 0) {
						fprintf(stderr, _("error while detaching kernel driver: %s\n"), libusb_error_name(r));
					}
				}
				if ((r=libusb_claim_interface(handle, 0)) != 0) {
					fprintf(stderr, _("interface claim error: %s\n"), libusb_error_name(r));
					return -1;
				}
				(*ptdev)->h=handle;
				return 0;
			}
		}
	}
	fprintf(stderr, _("No P-Touch printer found on USB (remember to put switch to position E)\n"));
	libusb_free_device_list(devs, 1);
	return -1;
}

int ptouch_close(ptouch_dev ptdev)
{
	libusb_release_interface(ptdev->h, 0);
	libusb_close(ptdev->h);
	return 0;
}

int ptouch_send(ptouch_dev ptdev, uint8_t *data, int len)
{
	int r,tx;

	if (ptdev == NULL) {
		return -1;
	}
	if ((r=libusb_bulk_transfer(ptdev->h, 0x02, data, len, &tx, 0)) != 0) {
		fprintf(stderr, _("write error: %s\n"), libusb_error_name(r));
		return -1;
	}
	if (tx != len) {
		fprintf(stderr, _("write error: could send only %i of %i bytes\n"), tx, len);
		return -1;
	}
	return 0;
}

int ptouch_init(ptouch_dev ptdev)
{
	char cmd[]="\x1b\x40";		/* 1B 40 = ESC @ = INIT */
	return ptouch_send(ptdev, (uint8_t *)cmd, strlen(cmd));
}

int ptouch_rasterstart(ptouch_dev ptdev)
{
	char cmd[]="\x1b\x69\x52\x01";	/* 1B 69 52 01 = RASTER DATA */
	return ptouch_send(ptdev, (uint8_t *)cmd, strlen(cmd));
}

/* print an empty line */
int ptouch_lf(ptouch_dev ptdev)
{
	char cmd[]="\x5a";
	return ptouch_send(ptdev, (uint8_t *)cmd, strlen(cmd));
}

/* print and advance tape, but do not cut */
int ptouch_ff(ptouch_dev ptdev)
{
	char cmd[]="\x0c";
	return ptouch_send(ptdev, (uint8_t *)cmd, strlen(cmd));
}

/* print and cut tape */
int ptouch_eject(ptouch_dev ptdev)
{
	char cmd[]="\x1a";
	return ptouch_send(ptdev, (uint8_t *)cmd, strlen(cmd));
}

/* print a "cut here" mark (it's just a dashed line) */
#define CUTMARK_SPACING 5
int ptouch_cutmark(ptouch_dev ptdev)
{
	uint8_t buf[32];
	int i,len=16;

	for (i=0; i<CUTMARK_SPACING; i++) {
		ptouch_lf(ptdev);
	}
	ptouch_rasterstart(ptdev);
	buf[0]=0x47;
	buf[1]=len;
	buf[2]=0;
	memset(buf+3, 0, len);
	int offset=(64-ptouch_getmaxwidth(ptdev)/2);
	for (i=0; i<ptouch_getmaxwidth(ptdev); i++) {
		if ((i%8) <= 3) {	/* pixels 0-3 get set, 4-7 are unset */
			buf[3+15-((offset+i)/8)] |= 1<<((offset+i)%8);
		}
	}
	ptouch_send(ptdev, buf, len+3);
	for (i=0; i<CUTMARK_SPACING; i++) {
		ptouch_lf(ptdev);
	}
	return 0;
}

void ptouch_rawstatus(uint8_t raw[32])
{
	fprintf(stderr, _("debug: dumping raw status bytes\n"));
	for (int i=0; i<32; i++) {
		fprintf(stderr, "%02x ", raw[i]);
		if (((i+1) % 16) == 0) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n");
	return;
}

int ptouch_getstatus(ptouch_dev ptdev)
{
	char cmd[]="\x1b\x69\x53";
	uint8_t buf[32];
	int i, r, tx=0, tries=0;
	struct timespec w;

	ptouch_send(ptdev, (uint8_t *)cmd, strlen(cmd));
	while (tx == 0) {
		w.tv_sec=0;
		w.tv_nsec=100000000;	/* 0.1 sec */
		r=nanosleep(&w, NULL);
		if ((r=libusb_bulk_transfer(ptdev->h, 0x81, buf, 32, &tx, 0)) != 0) {
			fprintf(stderr, _("read error: %s\n"), libusb_error_name(r));
			return -1;
		}
		tries++;
		if (tries > 10) {
			fprintf(stderr, _("timeout while waiting for status response\n"));
			return -1;
		}
	}
	if (tx == 32) {
		if (buf[0]==0x80 && buf[1]==0x20) {
			memcpy(ptdev->raw, buf, 32);
			if (buf[8] != 0) {
				fprintf(stderr, _("Error 1 = %02x\n"), buf[8]);
			}
			if (buf[9] != 0) {
				fprintf(stderr, _("Error 2 = %02x\n"), buf[9]);
			}
			ptdev->tape_width_mm=buf[10];
			ptdev->tape_width_px=0;
			for (i=0; tape_info[i].mm > 0; i++) {
				if (tape_info[i].mm == buf[10]) {
					ptdev->tape_width_px=tape_info[i].px;
				}
			}
			if (ptdev->tape_width_px == 0) {
				fprintf(stderr, _("unknown tape width of %imm, please report this.\n"), buf[10]);
			}
			ptdev->media_type=buf[11];
			ptdev->status=buf[18];
			return 0;
		}
	}
	if (tx == 16) {
		fprintf(stderr, _("got only 16 bytes... wondering what they are:\n"));
		ptouch_rawstatus(buf);
	}
	if (tx != 32) {
		fprintf(stderr, _("read error: got %i instead of 32 bytes\n"), tx);
		return -1;
	}
	fprintf(stderr, _("strange status:\n"));
	ptouch_rawstatus(buf);
	fprintf(stderr, _("trying to flush junk\n"));
	if ((r=libusb_bulk_transfer(ptdev->h, 0x81, buf, 32, &tx, 0)) != 0) {
		fprintf(stderr, _("read error: %s\n"), libusb_error_name(r));
		return -1;
	}
	fprintf(stderr, _("got another %i bytes. now try again\n"), tx);
	return -1;
}

int ptouch_getmaxwidth(ptouch_dev ptdev)
{
	return ptdev->tape_width_px;
}

int ptouch_sendraster(ptouch_dev ptdev, uint8_t *data, int len)
{
	uint8_t buf[32];

	if (len > 16) {		/* PT-2430PC can not print more than 128 px */
		return -1;	/* as we support more devices, we need to check */
	}			/* how much pixels each device support */
	buf[0]=0x47;
	buf[1]=len;
	buf[2]=0;
	memcpy(buf+3, data, len);
	return ptouch_send(ptdev, buf, len+3);
}
