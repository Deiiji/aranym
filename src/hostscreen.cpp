/*
 * $Header$
 *
 * STanda 2001
 */

#include "sysdeps.h"
#include "hardware.h"
#include "cpu_emulation.h"
#include "memory.h"
#include "hostscreen.h"
#include "parameters.h"

#define DEBUG 0
#include "debug.h"

#include "gfxprimitives.h"


/*
void SelectVideoMode()
{
	SDL_Rect **modes;
	uint32 i;

	// Get available fullscreen/hardware modes
	modes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);

	// Check is there are any modes available
	if (modes == (SDL_Rect **) 0) {
		printf("No modes available!\n");
		exit(-1);
	}

	// Check if or resolution is restricted
	if (modes == (SDL_Rect **) - 1) {
		printf("All resolutions available.\n");
	}
	else {
		// Print valid modes
		printf("Available Modes\n");
		for (i = 0; modes[i]; ++i)
			printf("  %d x %d\n", modes[i]->w, modes[i]->h);
	}
}
*/


void HostScreen::setWindowSize(	uint32 width, uint32 height )
{
	this->width  = width;
	this->height = height;

	// SelectVideoMode();
	sdl_videoparams = SDL_HWSURFACE;
	if (fullscreen)
		sdl_videoparams |= SDL_FULLSCREEN;

	surf = SDL_SetVideoMode(width, height, 16, sdl_videoparams);
	SDL_WM_SetCaption(VERSION_STRING, "ARAnyM");
	D(bug("Line Length = %d", surf->pitch));
	D(bug("Must Lock? %s", SDL_MUSTLOCK(surf) ? "YES" : "NO"));

	// is the SDL_update needed?
	doUpdate = ( surf->flags & SDL_HWSURFACE ) == 0;

	renderBegin();

	VideoRAMBaseHost = (uint8 *) surf->pixels;
	InitVMEMBaseDiff(VideoRAMBaseHost, VideoRAMBase);
	D(bug("VideoRAM starts at %p (%08x)", VideoRAMBaseHost, VideoRAMBase));
	D(bug("surf->pixels = %x, getVideoSurface() = %x",
			VideoRAMBaseHost, SDL_GetVideoSurface()->pixels));

	renderEnd();

	D(bug("Pixel format:bitspp=%d, tmasks r=%04x g=%04x b=%04x"
			", tshifts r=%d g=%d b=%d"
			", tlosses r=%d g=%d b=%d",
		    surf->format->BitsPerPixel,
			surf->format->Rmask, surf->format->Gmask, surf->format->Bmask,
			surf->format->Rshift, surf->format->Gshift, surf->format->Bshift,
			surf->format->Rloss, surf->format->Gloss, surf->format->Bloss));
}


void  HostScreen::drawLine( int32 x1, int32 y1, int32 x2, int32 y2, uint16 pattern, uint32 color )
{
	//	linePattern( pattern );
	uint8 r,g,b,a; SDL_GetRGBA( color, surf->format, &r, &g, &b, &a);
	lineRGBA( surf, (int16)x1, (int16)y1, (int16)x2, (int16)y2, r,g,b,a ); // SDL_gfxPrimitives
}


extern "C" {
	static void getBinary( uint16 data, char *buffer ) {
		for( uint16 i=0; i<=15; i++ ) {
			buffer[i] = (data & 1)?'1':' ';
			data>>=1;
		}
		buffer[16]='\0';
	}
}


/**
 * Derived from the SDL_gfxPrimitives::boxColor(). The colors are in the destination surface format here.
 * The trivial cases optimalization removed.
 *
 * @author STanda
 **/
void HostScreen::gfxBoxColorPattern (int16 x1, int16 y1, int16 x2, int16 y2, uint16 *areaPattern, uint32 fgColor, uint32 bgColor)
{
	uint8 *pixel, *pixellast;
	int16 x, dx, dy;
	int16 pixx, pixy;
	int16 w,h,tmp;

	/* Order coordinates */
	if (x1>x2) {
		tmp=x1;
		x1=x2;
		x2=tmp;
	}
	if (y1>y2) {
		tmp=y1;
		y1=y2;
		y2=tmp;
	}

	/* Calculate width&height */
	w=x2-x1+1; // STanda //+1
	h=y2-y1;

	/* More variable setup */
	dx=w;
	dy=h;
	pixx = surf->format->BytesPerPixel;
	pixy = surf->pitch;
	pixel = ((uint8*)surf->pixels) + pixx * (int32)x1 + pixy * (int32)y1;
	pixellast = pixel + pixx*dx + pixy*dy;

	// STanda // FIXME here the pattern should be checked out of the loops for performance
	          // but for now it is good enough
	          // FIXME not tested on other then 2 BPP

	/* Draw */
	switch(surf->format->BytesPerPixel) {
		case 1:
			// STanda // the loop is the same as the for the 2 BPP
			pixy -= (pixx*dx);
			for (; pixel<=pixellast; pixel += pixy) {
				uint16 pattern = areaPattern ? areaPattern[ y1++ & 0xf ] : 0xffff; // STanda
				for (x=0; x<dx; x++) {
					*(uint8*)pixel = (( ( pattern & ( 1 << ( (x1+x) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					pixel += pixx;
				}
			}
			break;

			//for (; pixel<=pixellast; pixel += pixy) {
			//	memset(pixel,(uint8)fgColor,dx);
			//}
			// STanda
			break;
		case 2:
			pixy -= (pixx*dx);
			for (; pixel<=pixellast; pixel += pixy) {
				uint16 pattern = areaPattern ? areaPattern[ y1++ & 0xf ] : 0xffff; // STanda
				for (x=0; x<dx; x++) {
					*(uint16*)pixel = (( ( pattern & ( 1 << ( (x1+x) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					pixel += pixx;
				}
			}
			break;
		case 3:
			pixy -= (pixx*dx);
			for (; pixel<=pixellast; pixel += pixy) {
				uint16 pattern = areaPattern ? areaPattern[ y1++ & 0xf ] : 0xffff; // STanda
				for (x=0; x<dx; x++) {
					uint32 color = (( ( pattern & ( 1 << ( (x1+x) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
						pixel[0] = (color >> 16) & 0xff;
						pixel[1] = (color >> 8) & 0xff;
						pixel[2] = color & 0xff;
					} else {
						pixel[0] = color & 0xff;
						pixel[1] = (color >> 8) & 0xff;
						pixel[2] = (color >> 16) & 0xff;
					}
					pixel += pixx;
				}
			}
			break;
		default: /* case 4*/
			pixy -= (pixx*dx);
			for (; pixel<=pixellast; pixel += pixy) {
				uint16 pattern = areaPattern ? areaPattern[ y1++ & 0xf ] : 0xffff; // STanda
				for (x=0; x<dx; x++) {
					*(uint32*)pixel = (( ( pattern & ( 1 << ( (x1+x) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					pixel += pixx;
				}
			}
			break;
	}  // switch
}


/**
 * Derived from the SDL_gfxPrimitives::boxColor(). The color is in the destination surface format here.
 * The trivial cases optimalization removed.
 *
 * @author STanda
 **/
void HostScreen::gfxBoxColorPatternBgTrans(int16 x1, int16 y1, int16 x2, int16 y2, uint16 *areaPattern, uint32 color)
{
	uint8 *pixel, *pixellast;
	int16 x, dx, dy;
	int16 pixx, pixy;
	int16 w,h,tmp;

	/* Order coordinates */
	if (x1>x2) {
		tmp=x1;
		x1=x2;
		x2=tmp;
	}
	if (y1>y2) {
		tmp=y1;
		y1=y2;
		y2=tmp;
	}

	/* Calculate width&height */
	w=x2-x1+1;
	h=y2-y1;

	/* More variable setup */
	dx=w;
	dy=h;
	pixx = surf->format->BytesPerPixel;
	pixy = surf->pitch;
	pixel = ((uint8*)surf->pixels) + pixx * (int32)x1 + pixy * (int32)y1;
	pixellast = pixel + pixx*dx + pixy*dy;

	// STanda // FIXME here the pattern should be checked out of the loops for performance
	          // but for now it is good enough
	          // FIXME not tested on other then 2 BPP

	/* Draw */
	switch(surf->format->BytesPerPixel) {
		case 1:
			// STanda // the loop is the same as the for the 2 BPP
			pixy -= (pixx*dx);
			for (; pixel<=pixellast; pixel += pixy) {
				uint16 pattern = areaPattern ? areaPattern[ y1++ & 0xf ] : 0xffff; // STanda
				for (x=0; x<dx; x++) {
					if ( ( pattern & ( 1 << ( (x1+x) & 0xf ) )) != 0 ) // STanda
						*(uint8*)pixel = color;
					pixel += pixx;
				}
			}
			break;

			//for (; pixel<=pixellast; pixel += pixy) {
			//	memset(pixel,(uint8)color,dx);
			//}
			// STanda
			break;
		case 2:
			pixy -= (pixx*dx);
			for (; pixel<=pixellast; pixel += pixy) {
				uint16 pattern = areaPattern ? areaPattern[ y1++ & 0xf ] : 0xffff; // STanda
				for (x=0; x<dx; x++) {
					if ( ( pattern & ( 1 << ( (x1+x) & 0xf ) )) != 0 ) // STanda
						*(uint16*)pixel = color;
					pixel += pixx;
				}
			}
			break;
		case 3:
			pixy -= (pixx*dx);
			for (; pixel<=pixellast; pixel += pixy) {
				uint16 pattern = areaPattern ? areaPattern[ y1++ & 0xf ] : 0xffff; // STanda
				for (x=0; x<dx; x++) {
					if ( ( pattern & ( 1 << ( (x1+x) & 0xf ) )) != 0 )
						if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
							pixel[0] = (color >> 16) & 0xff;
							pixel[1] = (color >> 8) & 0xff;
							pixel[2] = color & 0xff;
						} else {
							pixel[0] = color & 0xff;
							pixel[1] = (color >> 8) & 0xff;
							pixel[2] = (color >> 16) & 0xff;
						}
					pixel += pixx;
				}
			}
			break;
		default: /* case 4*/
			pixy -= (pixx*dx);
			for (; pixel<=pixellast; pixel += pixy) {
				uint16 pattern = areaPattern ? areaPattern[ y1++ & 0xf ] : 0xffff; // STanda
				for (x=0; x<dx; x++) {
					if ( ( pattern & ( 1 << ( (x1+x) & 0xf ) )) != 0 ) // STanda
						*(uint32*)pixel = color;
					pixel += pixx;
				}
			}
			break;
	}  // switch
}



/*
 * $Log$
 * Revision 1.7  2001/09/05 15:06:41  joy
 * SelectVideoMode() commented out.
 *
 * Revision 1.6  2001/09/04 13:51:45  joy
 * debug disabled
 *
 * Revision 1.5  2001/08/30 14:04:59  standa
 * The fVDI driver. mouse_draw implemented. Partial pattern fill support.
 * Still buggy.
 *
 * Revision 1.4  2001/08/28 23:26:09  standa
 * The fVDI driver update.
 * VIDEL got the doRender flag with setter setRendering().
 *       The host_colors_uptodate variable name was changed to hostColorsSync.
 * HostScreen got the doUpdate flag cleared upon initialization if the HWSURFACE
 *       was created.
 * fVDIDriver got first version of drawLine and fillArea (thanks to SDL_gfxPrimitives).
 *
 * Revision 1.3  2001/08/13 22:29:06  milan
 * IDE's params from aranymrc file etc.
 *
 * Revision 1.2  2001/07/24 09:36:51  joy
 * D(bug) macro replaces fprintf
 *
 * Revision 1.1  2001/06/18 13:21:55  standa
 * Several template.cpp like comments were added.
 * HostScreen SDL encapsulation class.
 *
 *
 */
