/*
 * Copyright 2001-2004 Red Hat Inc., Durham, North Carolina.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL RED HAT AND/OR THEIR SUPPLIERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Authors:
 *   David H. Dawes <dawes@xfree86.org>
 *   Kevin E. Martin <kem@redhat.com>
 *   Rickard E. (Rik) Faith <faith@redhat.com>
 *
 */

/** \file
 * This file contains code than supports cursor movement, including the
 * code that initializes and reinitializes the screen positions and
 * computes screen overlap.
 *
 * "This code is based very closely on the XFree86 equivalent
 * (xfree86/common/xf86Cursor.c)."  --David Dawes.
 *
 * "This code was then extensively re-written, as explained here."
 * --Rik Faith
 *
 * The code in xf86Cursor.c used edge lists to implement the
 * CursorOffScreen function.  The edge list computation was complex
 * (especially in the face of arbitrarily overlapping screens) compared
 * with the speed savings in the CursorOffScreen function.  The new
 * implementation has erred on the side of correctness, readability, and
 * maintainability over efficiency.  For the common (non-edge) case, the
 * dmxCursorOffScreen function does avoid a loop over all the screens.
 * When the cursor has left the screen, all the screens are searched,
 * and the first screen (in dmxScreens order) containing the cursor will
 * be returned.  If run-time profiling shows that this routing is a
 * performance bottle-neck, then an edge list may have to be
 * reimplemented.  An edge list algorithm is O(edges) whereas the new
 * algorithm is O(dmxNumScreens).  Since edges is usually 1-3 and
 * dmxNumScreens may be 30-60 for large backend walls, this trade off
 * may be compelling.
 *
 * The xf86InitOrigins routine uses bit masks during the computation and
 * is therefore limited to the length of a word (e.g., 32 or 64 bits)
 * screens.  Because Xdmx is expected to be used with a large number of
 * backend displays, this limitation was removed.  The new
 * implementation has erred on the side of readability over efficiency,
 * using the dmxSL* routines to manage a screen list instead of a
 * bitmap, and a function call to decrease the length of the main
 * routine.  Both algorithms are of the same order, and both are called
 * only at server generation time, so trading clarity and long-term
 * maintainability for efficiency does not seem justified in this case.
 */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#define DMX_CURSOR_DEBUG 0

#include "dmx.h"
#include "dmxsync.h"
#include "dmxcursor.h"
#include "dmxlog.h"
#include "dmxprop.h"
#include "dmxinput.h"
#include "dmxgrab.h"

#include "mipointer.h"
#include "windowstr.h"
#include "globals.h"
#include "cursorstr.h"
#include "dixevents.h"          /* For GetSpriteCursor() */

#if DMX_CURSOR_DEBUG
#define DMXDBG0(f)               dmxLog(dmxDebug,f)
#define DMXDBG1(f,a)             dmxLog(dmxDebug,f,a)
#define DMXDBG2(f,a,b)           dmxLog(dmxDebug,f,a,b)
#define DMXDBG3(f,a,b,c)         dmxLog(dmxDebug,f,a,b,c)
#define DMXDBG4(f,a,b,c,d)       dmxLog(dmxDebug,f,a,b,c,d)
#define DMXDBG5(f,a,b,c,d,e)     dmxLog(dmxDebug,f,a,b,c,d,e)
#define DMXDBG6(f,a,b,c,d,e,g)   dmxLog(dmxDebug,f,a,b,c,d,e,g)
#define DMXDBG7(f,a,b,c,d,e,g,h) dmxLog(dmxDebug,f,a,b,c,d,e,g,h)
#else
#define DMXDBG0(f)
#define DMXDBG1(f,a)
#define DMXDBG2(f,a,b)
#define DMXDBG3(f,a,b,c)
#define DMXDBG4(f,a,b,c,d)
#define DMXDBG5(f,a,b,c,d,e)
#define DMXDBG6(f,a,b,c,d,e,g)
#define DMXDBG7(f,a,b,c,d,e,g,h)
#endif

/** Initialize the private area for the cursor functions. */
Bool dmxInitCursor(ScreenPtr pScreen)
{
    if (!dixRequestPrivate(pScreen, sizeof(dmxCursorPrivRec)))
	return FALSE;

    if (!dixRequestPrivate (dmxDevicePrivateKey, sizeof(dmxDevicePrivRec)))
	return FALSE;

    return TRUE;
}

static Bool dmxCursorOffScreen(ScreenPtr *ppScreen, int *x, int *y)
{
    return FALSE;
}

static void dmxCrossScreen(ScreenPtr pScreen, Bool entering)
{
}

static void dmxWarpCursor(DeviceIntPtr pDev, ScreenPtr pScreen, int x, int y)
{
    int i;

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (!dmxScreen->beDisplay)
	    continue;

	XLIB_PROLOGUE (dmxScreen);
	XWarpPointer (dmxScreen->beDisplay, None, dmxScreen->scrnWin,
		      0, 0, 0, 0, x, y);
	XLIB_EPILOGUE (dmxScreen);
    }
}

miPointerScreenFuncRec dmxPointerCursorFuncs = {
    dmxCursorOffScreen,
    dmxCrossScreen,
    dmxWarpCursor
};

#ifdef ARGB_CURSOR

static Cursor
dmxCreateARGBCursor (ScreenPtr pScreen,
		     CursorPtr pCursor);

#endif

/** Create \a pCursor on the back-end associated with \a pScreen. */
void dmxBECreateCursor(ScreenPtr pScreen, CursorPtr pCursor)
{
    DMXScreenInfo    *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxCursorPrivPtr  pCursorPriv = DMX_GET_CURSOR_PRIV(pCursor, pScreen);
    CursorBitsPtr     pBits = pCursor->bits;
    Pixmap            src, msk;
    XColor            fg, bg;
    XImage           *img;
    XlibGC            gc = NULL;
    XGCValues         v;
    unsigned long     m;
    int               i;

    if (pCursorPriv->cursor)
	return;

    if (IsAnimCur (pCursor))
    {
	AnimCurPtr  ac = GetAnimCur (pCursor);
	XAnimCursor *cursors;
	int         i;

	cursors = xalloc (sizeof (*cursors) * ac->nelt);
	if (!cursors)
	    return;

	for (i = 0; i < ac->nelt; i++)
	{
	    dmxCursorPrivPtr pEltPriv = DMX_GET_CURSOR_PRIV (ac->elts[i].pCursor,
							     pScreen);

	    dmxBECreateCursor (pScreen, ac->elts[i].pCursor);

	    cursors[i].cursor = pEltPriv->cursor;
	    cursors[i].delay  = ac->elts[i].delay;
	}

	pCursorPriv->cursor = XRenderCreateAnimCursor (dmxScreen->beDisplay,
						       ac->nelt,
						       cursors);

	xfree (cursors);

	if (pCursorPriv->cursor)
	    return;
    }

#ifdef ARGB_CURSOR
    if (pCursor->bits->argb)
    {
	pCursorPriv->cursor = dmxCreateARGBCursor (pScreen, pCursor);
	if (pCursorPriv->cursor)
	    return;
    }
#endif

    m = GCFunction | GCPlaneMask | GCForeground | GCBackground | GCClipMask;
    v.function = GXcopy;
    v.plane_mask = AllPlanes;
    v.foreground = 1L;
    v.background = 0L;
    v.clip_mask = None;

    for (i = 0; i < dmxScreen->beNumPixmapFormats; i++) {
	if (dmxScreen->bePixmapFormats[i].depth == 1) {
	    /* Create GC in the back-end servers */
	    XLIB_PROLOGUE (dmxScreen);
	    gc = XCreateGC(dmxScreen->beDisplay, dmxScreen->scrnDefDrawables[i],
			   m, &v);
	    XLIB_EPILOGUE (dmxScreen);
	    break;
	}
    }
    if (!gc)
        dmxLog(dmxFatal, "dmxRealizeCursor: gc not initialized\n");

    XLIB_PROLOGUE (dmxScreen);

    src = XCreatePixmap(dmxScreen->beDisplay, dmxScreen->scrnWin,
			pBits->width, pBits->height, 1);
    msk = XCreatePixmap(dmxScreen->beDisplay, dmxScreen->scrnWin,
			pBits->width, pBits->height, 1);

    img = XCreateImage(dmxScreen->beDisplay,
		       dmxScreen->beVisuals[dmxScreen->beDefVisualIndex].visual,
		       1, XYBitmap, 0, (char *)pBits->source,
		       pBits->width, pBits->height,
		       BitmapPad(dmxScreen->beDisplay), 0);

    XPutImage(dmxScreen->beDisplay, src, gc, img, 0, 0, 0, 0,
	      pBits->width, pBits->height);

    XFree(img);
  
    img = XCreateImage(dmxScreen->beDisplay,
		       dmxScreen->beVisuals[dmxScreen->beDefVisualIndex].visual,
		       1, XYBitmap, 0, (char *)pBits->mask,
		       pBits->width, pBits->height,
		       BitmapPad(dmxScreen->beDisplay), 0);

    XPutImage(dmxScreen->beDisplay, msk, gc, img, 0, 0, 0, 0,
	      pBits->width, pBits->height);

    XFree(img);

    fg.red   = pCursor->foreRed;
    fg.green = pCursor->foreGreen;
    fg.blue  = pCursor->foreBlue;

    bg.red   = pCursor->backRed;
    bg.green = pCursor->backGreen;
    bg.blue  = pCursor->backBlue;

    pCursorPriv->cursor = XCreatePixmapCursor(dmxScreen->beDisplay,
					      src, msk,
					      &fg, &bg,
					      pBits->xhot, pBits->yhot);

    XFreePixmap(dmxScreen->beDisplay, src);
    XFreePixmap(dmxScreen->beDisplay, msk);
    XFreeGC(dmxScreen->beDisplay, gc);

    XLIB_EPILOGUE (dmxScreen);
}

static Bool _dmxRealizeCursor(ScreenPtr pScreen, CursorPtr pCursor)
{
    DMXScreenInfo    *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxCursorPrivPtr  pCursorPriv;

    DMXDBG2("_dmxRealizeCursor(%d,%p)\n", pScreen->myNum, pCursor);

    pCursorPriv = DMX_GET_CURSOR_PRIV(pCursor, pScreen);
    pCursorPriv->cursor = (Cursor)0;

    if (dmxScreen->beDisplay)
	dmxBECreateCursor(pScreen, pCursor);

    return TRUE;
}

/** Free \a pCursor on the back-end associated with \a pScreen. */
Bool dmxBEFreeCursor(ScreenPtr pScreen, CursorPtr pCursor)
{
    DMXScreenInfo    *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxCursorPrivPtr  pCursorPriv = DMX_GET_CURSOR_PRIV(pCursor, pScreen);

    if (pCursorPriv->cursor)
    {
	XLIB_PROLOGUE (dmxScreen);
	XFreeCursor(dmxScreen->beDisplay, pCursorPriv->cursor);
	XLIB_EPILOGUE (dmxScreen);
	pCursorPriv->cursor = (Cursor) 0;

	if (IsAnimCur (pCursor))
	{
	    AnimCurPtr ac = GetAnimCur (pCursor);
	    int        i;

	    for (i = 0; i < ac->nelt; i++)
		dmxBEFreeCursor (pScreen, ac->elts[i].pCursor);
	}

	return TRUE;
    }

    return FALSE;
}

static Bool
_dmxUnrealizeCursor (ScreenPtr pScreen, CursorPtr pCursor)
{
    DMXDBG2("_dmxUnrealizeCursor(%d,%p)\n",
            pScreen->myNum, pCursor);

    dmxBEFreeCursor(pScreen, pCursor);

    return TRUE;
}

static Bool
dmxRealizeCursor(DeviceIntPtr pDev, ScreenPtr pScreen, CursorPtr pCursor)
{
    if (pDev == inputInfo.pointer)
	return _dmxRealizeCursor (pScreen, pCursor);

    return TRUE;
}

static Bool
dmxUnrealizeCursor (DeviceIntPtr pDev,
		    ScreenPtr    pScreen,
		    CursorPtr    pCursor)
{
    if (pDev == inputInfo.pointer)
	return _dmxUnrealizeCursor (pScreen, pCursor);

    return TRUE;
}

static void
dmxMoveCursor (DeviceIntPtr pDev,
	       ScreenPtr    pScreen,
	       int          x,
	       int          y)
{
}

static void
dmxSetCursor (DeviceIntPtr pDev,
	      ScreenPtr    pScreen,
	      CursorPtr    pCursor,
	      int          x,
	      int          y)
{
}

static Bool
dmxDeviceCursorInitialize (DeviceIntPtr pDev,
			   ScreenPtr    pScr)
{
    return TRUE;
}

static void
dmxDeviceCursorCleanup (DeviceIntPtr pDev,
			ScreenPtr    pScr)
{
}

miPointerSpriteFuncRec dmxPointerSpriteFuncs =
{
    dmxRealizeCursor,
    dmxUnrealizeCursor,
    dmxSetCursor,
    dmxMoveCursor,
    dmxDeviceCursorInitialize,
    dmxDeviceCursorCleanup
};

#ifdef ARGB_CURSOR

#include <X11/extensions/Xrender.h>

static Cursor
dmxCreateARGBCursor (ScreenPtr pScreen,
		     CursorPtr pCursor)
{
    Pixmap	      xpixmap;
    XlibGC	      xgc;
    XImage	      *ximage;
    XRenderPictFormat *xformat;
    Picture	      xpicture;
    Cursor	      cursor = None;
    DMXScreenInfo     *dmxScreen = &dmxScreens[pScreen->myNum];

    XLIB_PROLOGUE (dmxScreen);

    xpixmap = XCreatePixmap (dmxScreen->beDisplay,
			     dmxScreen->scrnWin,
			     pCursor->bits->width,
			     pCursor->bits->height,
			     32);

    xgc = XCreateGC (dmxScreen->beDisplay, xpixmap, 0, NULL);

    ximage = XCreateImage (dmxScreen->beDisplay,
			   DefaultVisual (dmxScreen->beDisplay, 0),
			   32, ZPixmap, 0,
			   (char *) pCursor->bits->argb,
			   pCursor->bits->width,
			   pCursor->bits->height,
			   32, pCursor->bits->width * 4);

    XPutImage (dmxScreen->beDisplay, xpixmap, xgc, ximage,
	       0, 0, 0, 0, pCursor->bits->width, pCursor->bits->height);

    XFree (ximage);
    XFreeGC (dmxScreen->beDisplay, xgc);

    xformat = XRenderFindStandardFormat (dmxScreen->beDisplay,
					 PictStandardARGB32);
    xpicture = XRenderCreatePicture (dmxScreen->beDisplay, xpixmap,
				     xformat, 0, 0);

    cursor = XRenderCreateCursor (dmxScreen->beDisplay, xpicture,
				  pCursor->bits->xhot,
				  pCursor->bits->yhot);

    XRenderFreePicture (dmxScreen->beDisplay, xpicture);
    XFreePixmap (dmxScreen->beDisplay, xpixmap);

    XLIB_EPILOGUE (dmxScreen);

    return cursor;
}

#endif
