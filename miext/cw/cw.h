/*
 * Copyright © 2004 Eric Anholt
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Eric Anholt not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Eric Anholt makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * ERIC ANHOLT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ERIC ANHOLT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/* $Header$ */

#include "picturestr.h"

/*
 * One of these structures is allocated per GC that gets used with a window with
 * backing pixmap.
 */

typedef struct {
    GCPtr	    pBackingGC;	    /* Copy of the GC but with graphicsExposures
				     * set FALSE and the clientClip set to
				     * clip output to the valid regions of the
				     * backing pixmap. */
    int		    guarantee;      /* GuaranteeNothing, etc. */
    unsigned long   serialNumber;   /* clientClip computed time */
    unsigned long   stateChanges;   /* changes in parent gc since last copy */
    GCOps	    *wrapOps;	    /* wrapped ops */
    GCFuncs	    *wrapFuncs;	    /* wrapped funcs */
} cwGCRec, *cwGCPtr;

typedef struct {
    /*
     * screen func wrappers
     */
    CloseScreenProcPtr	CloseScreen;
    GetImageProcPtr	GetImage;
    GetSpansProcPtr	GetSpans;
    CreateGCProcPtr	CreateGC;
#ifdef RENDER
    CompositeProcPtr	Composite;
    GlyphsProcPtr	Glyphs;
#endif
} cwScreenRec, *cwScreenPtr;

/* cw.c */
DrawablePtr
cwGetBackingDrawable(DrawablePtr pDrawable, int *x_off, int *y_off);

/* cw_render.c */
void
cwComposite(CARD8	op,
	    PicturePtr	pSrc,
	    PicturePtr	pMask,
	    PicturePtr	pDst,
	    INT16	xSrc,
	    INT16	ySrc,
	    INT16	xMask,
	    INT16	yMask,
	    INT16	xDst,
	    INT16	yDst,
	    CARD16	width,
	    CARD16	height);

void
cwGlyphs(CARD8		op,
	 PicturePtr	pSrc,
	 PicturePtr	pDst,
	 PictFormatPtr	maskFormat,
	 INT16		xSrc,
	 INT16		ySrc,
	 int		nlist,
	 GlyphListPtr	list,
	 GlyphPtr	*glyphs);


void
miInitializeCompositeWrapper(ScreenPtr pScreen);
