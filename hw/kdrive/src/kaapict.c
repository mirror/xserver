/*
 * $RCSId$
 *
 * Copyright © 2001 Keith Packard
 *
 * Partly based on code that is Copyright © The XFree86 Project Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "kdrive.h"
#include "kaa.h"

#ifdef RENDER
#include "mipict.h"

#define KAA_DEBUG_FALLBACKS 0

#if KAA_DEBUG_FALLBACKS
static void
kaaPrintCompositeFallback(CARD8 op,
			  PicturePtr pSrc,
			  PicturePtr pMask,
			  PicturePtr pDst)
{
    char sop[20], ssfmt[20], smfmt[20], sdfmt[20];
    char sloc, mloc, dloc;
    int temp;

    switch(op)
    {
    case PictOpSrc:
	sprintf(sop, "Src");
	break;
    case PictOpOver:
	sprintf(sop, "Over");
	break;
    default:
	sprintf(sop, "0x%x", (int)op);
	break;
    }
    switch(pSrc->format)
    {
    case PICT_a8r8g8b8:
	sprintf(ssfmt, "ARGB8888");
	break;
    default:
	sprintf(ssfmt, "0x%x", (int)pSrc->format);
	break;
    }
    sprintf(smfmt, "(none)");
    if (pMask) {
	switch(pMask->format)
	{
	case PICT_a8:
	    sprintf(smfmt, "A8");
	    break;
	default:
	    sprintf(smfmt, "0x%x", (int)pMask->format);
	    break;
	}
    }
    switch(pDst->format)
    {
    case PICT_r5g6b5:
	sprintf(sdfmt, "RGB565");
	break;
    default:
	sprintf(sdfmt, "0x%x", (int)pDst->format);
	break;
    }
    strcpy (smfmt, ("None"));
    pMask = 0x0;

    sloc = kaaGetOffscreenPixmap(pSrc->pDrawable, &temp, &temp) ? 's' : 'm';
    mloc = (pMask && kaaGetOffscreenPixmap(pMask->pDrawable, &temp, &temp)) ?
	    's' : 'm';
    dloc = kaaGetOffscreenPixmap(pDst->pDrawable, &temp, &temp) ? 's' : 'm';

    ErrorF("Composite fallback: op %s, src 0x%x (%c), mask 0x%x (%c), "
	   "dst 0x%x (%c)\n"
	   "                    srcfmt %s, maskfmt %s, dstfmt %s\n",
	   sop, pSrc, sloc, pMask, mloc, pDst, dloc, ssfmt, smfmt, sdfmt);
}
#endif

void
kaaComposite(CARD8	op,
	     PicturePtr pSrc,
	     PicturePtr pMask,
	     PicturePtr pDst,
	     INT16	xSrc,
	     INT16	ySrc,
	     INT16	xMask,
	     INT16	yMask,
	     INT16	xDst,
	     INT16	yDst,
	     CARD16	width,
	     CARD16	height)
{
    KdScreenPriv (pDst->pDrawable->pScreen);
    KaaScreenPriv (pDst->pDrawable->pScreen);
    
    if (!pMask)
    {
	if (op == PictOpSrc)
	{
	    /*
	     * Check for two special cases -- solid fill and copy area
	     */
	    if (pSrc->pDrawable->width == 1 && pSrc->pDrawable->height == 1 &&
		pSrc->repeat)
	    {
		;
	    }
	    else if (!pSrc->repeat && pSrc->format == pDst->format)
	    {
		RegionRec	region;
		
		xDst += pDst->pDrawable->x;
		yDst += pDst->pDrawable->y;
		xSrc += pSrc->pDrawable->x;
		ySrc += pSrc->pDrawable->y;

		if (!miComputeCompositeRegion (&region, pSrc, pMask, pDst,
					       xSrc, ySrc, xMask, yMask, xDst, yDst,
					       width, height))
		    return;
		
		
		kaaCopyNtoN (pSrc->pDrawable, pDst->pDrawable, 0,
			     REGION_RECTS(&region), REGION_NUM_RECTS(&region),
			     xSrc - xDst, ySrc - yDst,
			     FALSE, FALSE, 0, 0);
		return;
	    }
	}

	if (pScreenPriv->enabled && pKaaScr->info->PrepareBlend)
	{
	    RegionRec region;
	    BoxPtr pbox;
	    int nbox;
	    int src_off_x, src_off_y;
	    int dst_off_x, dst_off_y;
	    PixmapPtr pSrcPix, pDstPix;

	    xDst += pDst->pDrawable->x;
	    yDst += pDst->pDrawable->y;

	    xSrc += pSrc->pDrawable->x;
	    ySrc += pSrc->pDrawable->y;

	    if (!miComputeCompositeRegion (&region, pSrc, pMask, pDst,
					   xSrc, ySrc, xMask, yMask, xDst, yDst,
					   width, height))
		return;


	    /* Migrate pixmaps to same place as destination */
	    if (pSrc->pDrawable->type == DRAWABLE_PIXMAP)
		kaaPixmapUseScreen ((PixmapPtr) pSrc->pDrawable);
	    if (pDst->pDrawable->type == DRAWABLE_PIXMAP)
		kaaPixmapUseScreen ((PixmapPtr) pDst->pDrawable);
	    
	    pSrcPix = kaaGetOffscreenPixmap (pSrc->pDrawable, &src_off_x,
					     &src_off_y);
	    pDstPix = kaaGetOffscreenPixmap (pDst->pDrawable, &dst_off_x,
					     &dst_off_y);
	    if (!pSrcPix || !pDstPix ||
		!(*pKaaScr->info->PrepareBlend) (op, pSrc, pDst, pSrcPix,
						 pDstPix))
	    {
		goto software;
	    }
	    
	    nbox = REGION_NUM_RECTS(&region);
	    pbox = REGION_RECTS(&region);

	    xSrc -= xDst;
	    ySrc -= yDst;
	
	    while (nbox--)
	    {
		(*pKaaScr->info->Blend) (pbox->x1 + xSrc + src_off_x,
					 pbox->y1 + ySrc + src_off_y,
					 pbox->x1 + dst_off_x,
					 pbox->y1 + dst_off_y,
					 pbox->x2 - pbox->x1,
					 pbox->y2 - pbox->y1);
		pbox++;
	    }
	    
	    (*pKaaScr->info->DoneBlend) ();
	    KdMarkSync(pDst->pDrawable->pScreen);

	    return;
	}
    }

software:
#if KAA_DEBUG_FALLBACKS
    kaaPrintCompositeFallback (op, pSrc, pMask, pDst);
#endif

    if (pSrc->pDrawable->type == DRAWABLE_PIXMAP)
	kaaPixmapUseMemory ((PixmapPtr) pSrc->pDrawable);
    if (pMask && pMask->pDrawable->type == DRAWABLE_PIXMAP)
	kaaPixmapUseMemory ((PixmapPtr) pMask->pDrawable);
#if 0
    if (pDst->pDrawable->type == DRAWABLE_PIXMAP)
	kaaPixmapUseMemory ((PixmapPtr) pDst->pDrawable);
#endif
    
    KdCheckComposite (op, pSrc, pMask, pDst, xSrc, ySrc, 
		      xMask, yMask, xDst, yDst, width, height);
}
#endif
