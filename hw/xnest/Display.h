/* $Xorg: Display.h,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

Copyright 1993 by Davor Matic

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation.  Davor Matic makes no representations about
the suitability of this software for any purpose.  It is provided "as
is" without express or implied warranty.

*/
/* $XFree86: xc/programs/Xserver/hw/xnest/Display.h,v 1.6 2001/01/17 22:36:55 dawes Exp $ */

#ifndef XNESTCOMMON_H
#define XNESTCOMMON_H

#define UNDEFINED -1

#define MAXDEPTH 32
#define MAXVISUALSPERDEPTH 256

extern XCBConnection    *xnestConnection;
extern XCBVISUALTYPE   **xnestVisuals;
extern CARD8            *xnestDepthForVisual;
extern int               xnestNumVisuals;
extern int               xnestDefaultVisualIndex;
extern XCBCOLORMAP      *xnestDefaultColormaps;
extern int               xnestNumDefaultColormaps;
extern CARD8            *xnestDepths;
extern int               xnestNumDepth;
extern XCBFORMAT        *xnestPixmapFormats;
extern int               xnestNumPixmapFormats;
extern CARD32            xnestBlackPixel;
extern CARD32            xnestWhitePixel;
extern XCBDRAWABLE       xnestDefaultDrawables[MAXDEPTH + 1];
extern XCBPIXMAP         xnestIconBitmap;
extern XCBPIXMAP         xnestScreenSaverPixmap;
extern XCBGCONTEXT       xnestBitmapGC;
extern unsigned long     xnestEventMask;

void xnestOpenDisplay(int argc, char *argv[]);
void xnestCloseDisplay(void);


#endif /* XNESTCOMMON_H */
