/*
	NatFeat JPEG decoder

	ARAnyM (C) 2005 Patrice Mandin

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "parameters.h"
#include "nfjpeg.h"
#include "../../atari/nfjpeg/nfjpeg_nfapi.h"
#define ARANYM_NFJPEG
#include "../../atari/nfjpeg/jpgdh.h"
#undef ARANYM_NFJPEG

#include <SDL/SDL_rwops.h>
#include <SDL/SDL_endian.h>
#include <SDL/SDL_image.h>

#define DEBUG 1
#include "debug.h"

/*--- Defines ---*/

#define EINVFN	-32

/*--- Types ---*/

/*--- Constructor/destructor functions ---*/

JpegDriver::JpegDriver()
{
	memset(images, 0, sizeof(images));
	D(bug("nfjpeg: created"));
}

JpegDriver::~JpegDriver()
{
	int i;

	for (i=1;i<=MAX_NFJPEG_IMAGES;i++) {
		if (images[i].src) {
			SDL_FreeSurface(images[i].src);
			images[i].src=NULL;
		}
		images[i].used=SDL_FALSE;
	}
	D(bug("nfjpeg: destroyed"));
}

/*--- Public functions ---*/

char *JpegDriver::name()
{
	return "JPEG";
}

bool JpegDriver::isSuperOnly()
{
	return false;
}

int32 JpegDriver::dispatch(uint32 fncode)
{
	int32 ret;

	D(bug("nfjpeg: dispatch(%u)", fncode));
	ret = EINVFN;

	switch(fncode) {
		case GET_VERSION:
    		ret = ARANFJPEG_NFAPI_VERSION;
			break;
		case NFJPEG_OPENDRIVER:
			ret = open_driver(getParameter(0));
			break;
		case NFJPEG_CLOSEDRIVER:
			ret = close_driver(getParameter(0));
			break;
		case NFJPEG_GETIMAGEINFO:
			ret = get_image_info(getParameter(0));
			break;
		case NFJPEG_GETIMAGESIZE:
			ret = get_image_size(getParameter(0));
			break;
		case NFJPEG_DECODEIMAGE:
			ret = decode_image(getParameter(0),getParameter(1));
			break;
		default:
			D(bug("nfjpeg: unimplemented function #%d", fncode));
			break;
	}
	D(bug("nfjpeg: function returning with 0x%08x", ret));
	return ret;
}

int32 JpegDriver::open_driver(memptr jpeg_ptr)
{
	struct _JPGD_STRUCT *tmp;
	int i,j;

	D(bug("nfjpeg: open_driver()"));

	tmp = (struct _JPGD_STRUCT *)Atari2HostAddr(jpeg_ptr);

	/* Find a free handle */
	j=0;
	for (i=1;i<=MAX_NFJPEG_IMAGES;i++) {
		if (!images[i].used) {
			j=i;
			break;
		}
	}

	if (j==0) {
		return EINVFN;
	}

	tmp->handle = j;
	images[j].used = SDL_TRUE;
	return 0;
}

int32 JpegDriver::close_driver(memptr jpeg_ptr)
{
	struct _JPGD_STRUCT *tmp;

	D(bug("nfjpeg: close_driver()"));

	tmp = (struct _JPGD_STRUCT *)Atari2HostAddr(jpeg_ptr);

	if (images[tmp->handle].src) {
		SDL_FreeSurface(images[tmp->handle].src);
		images[tmp->handle].src=NULL;
	}
	images[tmp->handle].used=SDL_FALSE;
	return 0;
}

int32 JpegDriver::get_image_info(memptr jpeg_ptr)
{
	struct _JPGD_STRUCT *tmp;

	D(bug("nfjpeg: get_image_info()"));

	tmp = (struct _JPGD_STRUCT *)Atari2HostAddr(jpeg_ptr);

	if (images[tmp->handle].src == NULL) {
		/* Damn, we need to decode it with SDL_image */
		if (load_image(tmp, Atari2HostAddr(tmp->InPointer),SDL_SwapBE32(tmp->InSize))) {
			return 0;
		}
	}

	return EINVFN;
}

int32 JpegDriver::get_image_size(memptr jpeg_ptr)
{
	struct _JPGD_STRUCT *tmp;

	D(bug("nfjpeg: get_image_size()"));

	tmp = (struct _JPGD_STRUCT *)Atari2HostAddr(jpeg_ptr);

	if (images[tmp->handle].src == NULL) {
		/* Damn, we need to decode it with SDL_image */
		if (load_image(tmp, Atari2HostAddr(tmp->InPointer),SDL_SwapBE32(tmp->InSize))) {
			return 0;
		}
	}
	return EINVFN;
}

int32 JpegDriver::decode_image(memptr jpeg_ptr, uint32 row)
{
	struct _JPGD_STRUCT *tmp;
	unsigned char *dest,*src, *src_line;
	int width,height,r,g,b,x,y;
	SDL_Surface *surface;
	SDL_PixelFormat *format;

	D(bug("nfjpeg: decode_image()"));

	tmp = (struct _JPGD_STRUCT *)Atari2HostAddr(jpeg_ptr);

	if (images[tmp->handle].src == NULL) {
		/* Damn, we need to decode it with SDL_image */
		if (!load_image(tmp, Atari2HostAddr(tmp->InPointer),SDL_SwapBE32(tmp->InSize))) {
			return EINVFN;
		}
	}

	dest = (unsigned char *)Atari2HostAddr(tmp->OutTmpPointer);
	surface = images[tmp->handle].src;
	src = (unsigned char *)surface->pixels;
	src += surface->pitch * row * 16;
	width = SDL_SwapBE16(tmp->MFDBPixelWidth);
	height = 16;
	format = surface->format;

	switch(SDL_SwapBE16(tmp->OutPixelSize)) {
		case 1:	/* Luminance */
			for (y=0; y<height; y++) {
				unsigned char *dst_line;

				src_line = src;
				dst_line = dest;
				for (x=0;x<width;x++) {
					read_rgb(format, src_line, &r, &g, &b);
					*dst_line++ = (r*30+g*59+b*11)/100;
					src_line+=format->BytesPerPixel;
				}

				src += surface->pitch;
				dest += width;
			}
			break;
		case 2:	/* R5G6B5 (big endian) */
			for (y=0; y<height; y++) {
				unsigned short *dst_line;

				src_line = src;
				dst_line = (unsigned short *)dest;
				for (x=0;x<width;x++) {
					read_rgb(format, src_line, &r, &g, &b);
					*dst_line++ = SDL_SwapBE16(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
					src_line+=format->BytesPerPixel;
				}

				src += surface->pitch;
				dest += width<<1;
			}
			break;
		case 3:	/* R8G8B8 (big endian) */
			for (y=0; y<height; y++) {
				unsigned char *dst_line;

				src_line = src;
				dst_line = dest;
				for (x=0;x<width;x++) {
					read_rgb(format, src_line, &r, &g, &b);
					*dst_line++ = r;
					*dst_line++ = g;
					*dst_line++ = b;
					src_line+=format->BytesPerPixel;
				}

				src += surface->pitch;
				dest += width*3;
			}
			break;
		case 4:	/* FIXME A8R8G8B8 or R8G8B8A8 or something else ? */
			for (y=0; y<height; y++) {
				unsigned long *dst_line;

				src_line = src;
				dst_line = (unsigned long *)dest;
				for (x=0;x<width;x++) {
					read_rgb(format, src_line, &r, &g, &b);
					*dst_line++ = SDL_SwapBE32((r<<16)|(g<<8)|b);
					src_line+=format->BytesPerPixel;
				}

				src += surface->pitch;
				dest += width<<2;
			}
			break;
	}

	return 0;
}

SDL_bool JpegDriver::load_image(struct _JPGD_STRUCT *jpgd_ptr, uint8 *buffer, uint32 size)
{
	SDL_RWops *src;
	SDL_Surface *surface;
	int width, height, line_width, image_size;

	D(bug("nfjpeg: load_image()"));

	/* Load image from memory */
	src = SDL_RWFromMem(buffer, size);
	if (src==NULL) {
		return SDL_FALSE;
	}
	surface = IMG_Load_RW(src, 0);
#if DEBUG
	if (surface) {
		D(bug("nfjpeg: %dx%d image", surface->w, surface->h));
		D(bug("nfjpeg: R=0x%08x,G=0x%08x,B=0x%08x,A=0x%08x",
			surface->format->Rmask, surface->format->Gmask,
			surface->format->Bmask, surface->format->Amask
		));
	}
#endif
	SDL_FreeRW(src);
	if (surface==NULL) {
		return SDL_FALSE;
	}
	images[jpgd_ptr->handle].src = surface;

	/* Fill values */
	jpgd_ptr->InComponents = SDL_SwapBE16(3); /* RGB */

	width = surface->w;
	if ((width & 15)!=0) {
		width = (width | 15)+1;
	}
	jpgd_ptr->XLoopCounter = SDL_SwapBE16(width>>4);

	height = surface->h;
	if ((height & 15)!=0) {
		height = (height | 15)+1;
	}
	jpgd_ptr->YLoopCounter = SDL_SwapBE16(height>>4);

	jpgd_ptr->MFDBAddress = 0;

	jpgd_ptr->MFDBPixelWidth = SDL_SwapBE16(surface->w);

	jpgd_ptr->MFDBPixelHeight = SDL_SwapBE16(surface->h);

	line_width = surface->w;
	if ((line_width & 15)!=0) {	/* Must be multiple of 16 */
		line_width = (line_width | 15)+1;
	}
	jpgd_ptr->MFDBWordSize = SDL_SwapBE16(line_width);

	jpgd_ptr->MFDBFormatFlag = 0;

	jpgd_ptr->MFDBBitPlanes = SDL_SwapBE16(jpgd_ptr->OutPixelSize) * 8;

	image_size = line_width * surface->h;
	jpgd_ptr->OutSize = SDL_SwapBE32(image_size);

	jpgd_ptr->MFDBReserved1 = jpgd_ptr->MFDBReserved2 = jpgd_ptr->MFDBReserved3 = 0;

	return SDL_TRUE;
}

void JpegDriver::read_rgb(SDL_PixelFormat *format, void *src, int *r, int *g, int *b)
{
	unsigned long color;

	color = 0;
	switch(format->BytesPerPixel) {
		case 1:
			{
				unsigned char *tmp;
				
				tmp = (unsigned char *)src;
				color = *tmp;
			}
			break;
		case 2:
			{
				unsigned short *tmp;
				
				tmp = (unsigned short *)src;
				color = *tmp;
			}
			break;
		case 3:
			{
				unsigned char *tmp;
				
				tmp = (unsigned char *)src;
				color = (tmp[0]<<8)|(tmp[1]<<8)|tmp[2];
			}
			break;
		case 4:
			{
				unsigned long *tmp;
				
				tmp = (unsigned long *)src;
				color = *tmp;
			}
			break;
	}

	*r = color & (format->Rmask);
	*r >>= format->Rshift;
	*r <<= format->Rloss;
	*g = color & (format->Gmask);
	*g >>= format->Gshift;
	*g <<= format->Gloss;
	*b = color & (format->Bmask);
	*b >>= format->Bshift;
	*b <<= format->Bloss;
}
