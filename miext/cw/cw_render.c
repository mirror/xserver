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

#include "gcstruct.h"
#include "windowstr.h"
#include "cw.h"

#ifdef RENDER

extern int cwScreenIndex;

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
	    CARD16	height)
{
    ScreenPtr pScreen = pDst->pDrawable->pScreen;
    PictureScreenPtr ps = GetPictureScreenIfSet(pScreen);
    cwScreenPtr pScreenPriv =
	(cwScreenPtr)pScreen->devPrivates[cwScreenIndex].ptr;
    DrawablePtr pSrcDraw, pMaskDraw = NULL, pDstDraw;
    DrawablePtr pBackSrcDraw, pBackMaskDraw, pBackDstDraw;
    int src_off_x, src_off_y, mask_off_x, mask_off_y, dst_off_x, dst_off_y;

    pSrcDraw = pSrc->pDrawable;
    pBackSrcDraw = cwGetBackingDrawable(pSrcDraw, &src_off_x, &src_off_y);
    xSrc += src_off_x;
    ySrc += src_off_y;
    pSrc->pDrawable = pBackSrcDraw;

    pDstDraw = pDst->pDrawable;
    pBackDstDraw = cwGetBackingDrawable(pDstDraw, &dst_off_x, &dst_off_y);
    xDst += dst_off_x;
    yDst += dst_off_y;
    pDst->pDrawable = pBackDstDraw;

    if (pMask) {
	pMaskDraw = pMask->pDrawable;
	pBackMaskDraw = cwGetBackingDrawable(pMaskDraw, &mask_off_x,
					     &mask_off_y);
	xMask += mask_off_x;
	yMask += mask_off_y;
	pMask->pDrawable = pBackMaskDraw;
    }

    ps->Composite = pScreenPriv->Composite;
    (*ps->Composite)(op, pSrc, pMask, pDst, xSrc, ySrc, xMask, yMask,
		     xDst, yDst, width, height);
    ps->Composite = cwComposite;

    pSrc->pDrawable = pSrcDraw;
    pDst->pDrawable = pDstDraw;
    if (pMask)
	pMask->pDrawable = pMaskDraw;
}
void
cwGlyphs(CARD8		op,
	 PicturePtr	pSrc,
	 PicturePtr	pDst,
	 PictFormatPtr	maskFormat,
	 INT16		xSrc,
	 INT16		ySrc,
	 int		nlist,
	 GlyphListPtr	list,
	 GlyphPtr	*glyphs)
{
    ScreenPtr pScreen = pDst->pDrawable->pScreen;
    PictureScreenPtr ps = GetPictureScreenIfSet(pScreen);
    cwScreenPtr pScreenPriv =
	(cwScreenPtr)pScreen->devPrivates[cwScreenIndex].ptr;
    DrawablePtr pSrcDraw, pDstDraw;
    DrawablePtr pBackSrcDraw, pBackDstDraw;
    int src_off_x, src_off_y, dst_off_x, dst_off_y;
    GlyphListPtr oldList;

    pDstDraw = pDst->pDrawable;
    pBackDstDraw = cwGetBackingDrawable(pDstDraw, &dst_off_x, &dst_off_y);
    if (dst_off_x != 0 || dst_off_y != 0) {
	int i;

	oldList = list;
	list = ALLOCATE_LOCAL(nlist * sizeof(GlyphListRec));
	if (list == NULL)
	    return;
	memcpy(list, oldList, nlist * sizeof(GlyphListRec));
	for (i = 0; i < nlist; i++) {
	    list[i].xOff += dst_off_x;
	    list[i].yOff += dst_off_y;
	}
    }
    pDst->pDrawable = pBackDstDraw;

    pSrcDraw = pSrc->pDrawable;
    pBackSrcDraw = cwGetBackingDrawable(pSrcDraw, &src_off_x, &src_off_y);
    xSrc += src_off_x;
    ySrc += src_off_y;
    pSrc->pDrawable = pBackSrcDraw;

    ps->Glyphs = pScreenPriv->Glyphs;
    (*ps->Glyphs)(op, pSrc, pDst, maskFormat, xSrc, ySrc, nlist, list, glyphs);
    ps->Glyphs = cwGlyphs;

    pSrc->pDrawable = pSrcDraw;
    pDst->pDrawable = pDstDraw;
    if (dst_off_x != 0 || dst_off_y != 0)
	DEALLOCATE_LOCAL(list);
}

#endif /* RENDER */
