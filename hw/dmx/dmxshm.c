/*
 * Copyright © 2008 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxshm.h"
#include "dmxgc.h"
#include "dmxwindow.h"
#include "dmxpixmap.h"
#include "dmxsync.h"
#include "scrnintstr.h"
#include "servermd.h"
#include "shmint.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

#ifdef MITSHM

unsigned long DMX_SHMSEG;

extern int (*ProcShmVector[ShmNumberRequests])(ClientPtr);

static int (*dmxSaveProcVector[ShmNumberRequests]) (ClientPtr);

static void dmxShmPutImage (XSHM_PUT_IMAGE_ARGS);

static ShmFuncs dmxFuncs = { NULL, dmxShmPutImage };

void
ShmRegisterDmxFuncs (ScreenPtr pScreen)
{
    ShmRegisterFuncs (pScreen, &dmxFuncs);
}

static void
dmxShmPutImage (DrawablePtr  pDraw,
		GCPtr	     pGC,
		int	     depth,
		unsigned int format,
		int	     w,
		int	     h,
		int	     sx,
		int	     sy,
		int	     sw,
		int	     sh,
		int	     dx,
		int	     dy,
		char	     *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDraw->pScreen->myNum];
    dmxGCPrivPtr  pGCPriv = DMX_GET_GC_PRIV (pGC);
    int           vIndex = dmxScreen->beDefVisualIndex;
    XImage        *image = NULL;
    Drawable      draw;

    if (pDraw->type == DRAWABLE_WINDOW)
	draw = (DMX_GET_WINDOW_PRIV ((WindowPtr) (pDraw)))->window;
    else
	draw = (DMX_GET_PIXMAP_PRIV ((PixmapPtr) (pDraw)))->pixmap;

    if (!dmxScreen->beDisplay || !draw)
	return;

    XLIB_PROLOGUE (dmxScreen);
    image = XCreateImage (dmxScreen->beDisplay,
			  dmxScreen->beVisuals[vIndex].visual,
			  depth, format, 0, data, w, h,
			  BitmapPad (dmxScreen->beDisplay),
			  (format == ZPixmap) ?
			  PixmapBytePad (w, depth) : BitmapBytePad (w));
    XLIB_EPILOGUE (dmxScreen);

    if (image)
    {
	RegionRec reg;
	BoxRec    src, dst;
	BoxPtr    pBox;
	int       nBox;

	src.x1 = dx - sx;
	src.y1 = dy - sy;
	src.x2 = src.x1 + w;
	src.y2 = src.y1 + h;

	dst.x1 = dx;
	dst.y1 = dy;
	dst.x2 = dst.x1 + sw;
	dst.y2 = dst.y1 + sh;

	if (src.x1 > dst.x1)
	    dst.x1 = src.x1;
	if (src.y1 > dst.y1)
	    dst.y1 = src.y1;
	if (src.x2 < dst.x2)
	    dst.x2 = src.x2;
	if (src.y2 < dst.y2)
	    dst.y2 = src.y2;

	REGION_INIT (pGC->pScreen, &reg, &dst, 0);

	if (pGC->pCompositeClip)
	{
	    REGION_TRANSLATE (pGC->pScreen, pGC->pCompositeClip,
			      -pDraw->x, -pDraw->y);
	    REGION_INTERSECT (pGC->pScreen, &reg, &reg, pGC->pCompositeClip);
	    REGION_TRANSLATE (pGC->pScreen, pGC->pCompositeClip,
			      pDraw->x, pDraw->y);
	}

	nBox = REGION_NUM_RECTS (&reg);
	pBox = REGION_RECTS (&reg);

	XLIB_PROLOGUE (dmxScreen);
	while (nBox--)
	{
	    XPutImage (dmxScreen->beDisplay, draw, pGCPriv->gc, image,
		       pBox->x1 - src.x1,
		       pBox->y1 - src.y1,
		       pBox->x1,
		       pBox->y1,
		       pBox->x2 - pBox->x1,
		       pBox->y2 - pBox->y1);
	    pBox++;
	}
	XLIB_EPILOGUE (dmxScreen);

	REGION_UNINIT (pGC->pScreen, &reg);

	XFree (image);

	dmxSync (dmxScreen, FALSE);
    }
}

static int
dmxFreeShmSeg (pointer value,
	       XID     id)
{
    return Success;
}

static int
dmxProcShmAttach (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ShmAttach]) (client);
    if (err != Success)
	return err;

    return Success;
}

static int
dmxProcShmDetach (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ShmDetach]) (client);
    if (err != Success)
	return err;

    return Success;
}

static int
dmxProcShmGetImage (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ShmGetImage]) (client);
    if (err != Success)
	return err;

    return Success;
}

static int
dmxProcShmPutImage (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ShmPutImage]) (client);
    if (err != Success)
	return err;

    return Success;
}

void dmxInitShm (void)
{
    int i;

    DMX_SHMSEG = CreateNewResourceType (dmxFreeShmSeg);

    for (i = 0; i < ShmNumberRequests; i++)
	dmxSaveProcVector[i] = ProcShmVector[i];

    ProcShmVector[X_ShmAttach]   = dmxProcShmAttach;
    ProcShmVector[X_ShmDetach]   = dmxProcShmDetach;
    ProcShmVector[X_ShmGetImage] = dmxProcShmGetImage;
    ProcShmVector[X_ShmPutImage] = dmxProcShmPutImage;
    ProcShmVector[X_ShmGetImage] = dmxProcShmGetImage;
}

void dmxResetShm (void)
{
    int i;

    for (i = 0; i < ShmNumberRequests; i++)
	ProcShmVector[i] = dmxSaveProcVector[i];
}

#endif
