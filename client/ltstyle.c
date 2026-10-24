//-----------------------------------------------------------------------------
//
//  $Logfile:: /Source/ltstyle.c                                              $
// $Revision:: 2                                                              $
//   $Author:: Burgermac                                                      $
//     $Date:: 12/27/00 7:47 PM                                               $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
// Lightstyle creator from a targa file.

#include "qcommon.h"
#include "q_shared.h"
#include "ltstyle.h"

typedef struct _TargaHeader {
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

/*
=============
LoadTGA
=============
*/
qboolean LoadTGA (const char *name, byte **pic, int *width, int *height, byte **palette, qboolean *hasalpha)
{
	int columns, rows, numPixels;
	byte *pixbuf;
	int row, column;
	byte *buf_p;
	void *buffer;
	TargaHeader targa_header;
	byte *targa_rgba;
	byte *targa_palette;
	int i;

	*pic = NULL;
	*palette = NULL;
	*hasalpha = false;

	//
	// load the file
	//
	if (FS_LoadFile(name, &buffer) < 0) {
		Com_Printf("TGA File not found: %s\n", name);
		return false;
	}

	buf_p = (byte *) buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index  = LittleShort(*(short *)buf_p); buf_p += 2;
	targa_header.colormap_length = LittleShort(*(short *)buf_p); buf_p += 2;
	targa_header.colormap_size = *buf_p++ / 8;
	targa_header.x_origin = LittleShort(*(short *)buf_p); buf_p += 2;
	targa_header.y_origin = LittleShort(*(short *)buf_p); buf_p += 2;
	targa_header.width    = LittleShort(*(short *)buf_p); buf_p += 2;
	targa_header.height   = LittleShort(*(short *)buf_p); buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type != 1 && targa_header.image_type != 2 && targa_header.image_type != 10) {
		Com_Printf("LoadTGA: Only type 1, 2 and 10 targa RGB images supported\n");
		return false;
	}

	if ((targa_header.colormap_type != 0 && targa_header.colormap_type != 1) ||
	    (targa_header.pixel_size != 32 && targa_header.pixel_size != 24 && targa_header.pixel_size != 8)) {
		Com_Printf("LoadTGA: Only 24, 32 and colormap images supported (no colormaps)\n");
		return false;
	}

	columns = targa_header.width;
	rows = targa_header.height;
	if (width) *width = columns;
	if (height) *height = rows;

	numPixels = columns * rows;
	if (targa_header.image_type != 1)
		numPixels *= 4;
	targa_rgba = (byte *) malloc(numPixels);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length; // skip TARGA image comment

	if (targa_header.image_type == 1) {
		targa_palette = (byte *) malloc(256*4);
		*palette = targa_palette;
		for (i = 0; i < targa_header.colormap_length; i++) {
			byte blue  = *buf_p++;
			byte green = *buf_p++;
			byte red   = *buf_p++;
			byte alpha = 0xff;

			if (targa_header.colormap_size == 4) {
				alpha = *buf_p++;
				if (alpha != 0xff)
					*hasalpha = true;
			}
			targa_palette[i*4+0] = red;
			targa_palette[i*4+1] = green;
			targa_palette[i*4+2] = blue;
			targa_palette[i*4+3] = alpha;
		}
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row*columns;
			for (column = 0; column < columns; column++) {
				switch (targa_header.pixel_size) {
				case 8:
					*pixbuf++ = *buf_p++;
					break;
				}
			}
		}
	}
	else if (targa_header.image_type == 2) { // Uncompressed, RGB images
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for (column = 0; column < columns; column++) {
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size) {
				case 24:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					if (alphabyte != 0xff)
						*hasalpha = true;
					break;
				}
			}
		}
	}
	else if (targa_header.image_type == 10) { // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for (column = 0; column < columns; ) {
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) { // run-length packet
					switch (targa_header.pixel_size) {
					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = 255;
						break;
					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						if (alphabyte != 0xff)
							*hasalpha = true;
						break;
					default: /* silence compiler */
						blue = red = green = alphabyte = 0;
						break;
					}

					for (j = 0; j < packetSize; j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns) { // run spans across rows
							column = 0;
							if (row > 0) row--;
							else goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else { // non run-length packet
					for (j = 0; j < packetSize; j++) {
						switch (targa_header.pixel_size) {
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							if (alphabyte != 0xff)
								*hasalpha = true;
							break;
						}
						column++;
						if (column == columns) { // pixel packet run spans across rows
							column = 0;
							if (row > 0) row--;
							else goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
			}
breakOut:   ;
		}
	}

	FS_FreeFile(buffer);
	return true;
}

char ltstyle_buffer[MAX_LIGHTSTYLE_STRINGLENGTH];

char *createLightstyleFromTGA (const char *filename)
{
	byte *data = NULL;
	byte *pal = NULL;
	byte *p;
	int width, height;
	qboolean hasalpha;
	byte red[MAX_LIGHTSTYLE_STRINGLENGTH/4],green[MAX_LIGHTSTYLE_STRINGLENGTH/4];
	byte blue[MAX_LIGHTSTYLE_STRINGLENGTH/4],alpha[MAX_LIGHTSTYLE_STRINGLENGTH/4];
	int i, count;
	char c;
	float s;

	if (!LoadTGA(filename, &data, &width, &height, &pal, &hasalpha))
		return NULL;

	p = data;

	if (width > (MAX_LIGHTSTYLE_STRINGLENGTH/4)-4)
	{
		Com_DPrintf(DEVELOPER_MSG_CLIENT, "createLightstyleFromTGA: Light style too large, truncating.\n");
		width = (MAX_LIGHTSTYLE_STRINGLENGTH/4)-4;
	}

	for (i = 0; i < width; i++) {
		if (pal) {
			int index = *(p++);
			red[i]   = pal[index*4 + 0];
			green[i] = pal[index*4 + 1];
			blue[i]  = pal[index*4 + 2];
			alpha[i] = pal[index*4 + 3];
		}
		else {
			red[i]   = *(p++);
			green[i] = *(p++);
			blue[i]  = *(p++);

			if (hasalpha)
				alpha[i] = *(p++);
			else {
				alpha[i] = 0xff;
				p++;
			}
		}
	}

	if (data) free(data);
	if (pal) free(pal);
	count = 0;

	ltstyle_buffer[count++] = 'R';
	for (i = 0; i < width; i++)
	{
		s = (float)red[i] / 255.0f;
		s *= 25;
		c = (int)s + 'a';
		ltstyle_buffer[count++] = c;
	}

	ltstyle_buffer[count++] = 'G';
	for (i = 0; i < width; i++)
	{
		s = (float)green[i] / 255.0f;
		s *= 25;
		c = (int)s + 'a';
		ltstyle_buffer[count++] = c;
	}

	ltstyle_buffer[count++] = 'B';
	for (i = 0; i < width; i++)
	{
		s = (float)blue[i] / 255.0f;
		s *= 25;
		c = (int)s + 'a';
		ltstyle_buffer[count++] = c;
	}

	if (hasalpha)
	{
		ltstyle_buffer[count++] = 'A';
		for (i = 0; i < width; i++)
		{
			s = (float)alpha[i] / 255.0f;
			s *= 25;
			c = (int)s + 'a';
			ltstyle_buffer[count++] = c;
		}
	}
	else
	{
		ltstyle_buffer[count] = 0;
	}

	return ltstyle_buffer;
}
