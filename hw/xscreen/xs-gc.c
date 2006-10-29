/* $Xorg: GC.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

   Copyright 2006 Ori Bernstein
   Permission to use, copy, modify, distribute, and sell this software
   and its documentation for any purpose is hereby granted without fee,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation. Ori Bernstein makes no representations about
   the suitability of this software for any purpose.  It is provided "as
   is" without express or implied warranty.

*/
/* $XFree86: xc/programs/Xserver/hw/xs/GC.c,v 3.6 2001/10/28 03:34:11 tsi Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xs-config.h>
#endif

#include <X11/Xmd.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_aux.h>
#include "gcstruct.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "scrnintstr.h"
#include <X11/fonts/fontstruct.h>
#include "mistruct.h"
#include "region.h"

#include "xs-globals.h"
#include "xs-pixmap.h"
#include "xs-font.h"
#include "xs-gcops.h"
#include "xs-gc.h"

int XS_GC_PRIVateIndex;

static GCFuncs xsFuncs = {
    xsValidateGC,
    xsChangeGC,
    xsCopyGC,
    xsDestroyGC,
    xsChangeClip,
    xsDestroyClip,
    xsCopyClip,
};

static GCOps xsOps = {
    xsFillSpans,
    xsSetSpans,
    xsPutImage,
    xsCopyArea, 
    xsCopyPlane,
    xsPolyPoint,
    xsPolylines,
    xsPolySegment,
    xsPolyRectangle,
    xsPolyArc,
    xsFillPolygon,
    xsPolyFillRect,
    xsPolyFillArc,
    xsPolyText8, 
    xsPolyText16,
    xsImageText8,
    xsImageText16,
    xsImageGlyphBlt,
    xsPolyGlyphBlt,
    xsPushPixels
};

Bool xsCreateGC(GCPtr pGC)
{
    pGC->clientClipType = CT_NONE;
    pGC->clientClip = NULL;

    pGC->funcs = &xsFuncs;
    pGC->ops = &xsOps;

    pGC->miTranslate = 1;

    XS_GC_PRIV(pGC)->gc = xcb_generate_id(xsConnection);
    xcb_create_gc(xsConnection, 
                XS_GC_PRIV(pGC)->gc,
                xsDefaultDrawables[pGC->depth],
                0L,
                NULL);
    //XS_GC_PRIV(pGC)->nClipRects = 0;

    return TRUE;
}

void xsValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr pDrawable)
{
    pGC->lastWinOrg.x = pDrawable->x;
    pGC->lastWinOrg.y = pDrawable->y;
}

void xsChangeGC(GCPtr pGC, unsigned long mask)
{
    xcb_params_gc_t values;

    if (mask & XCB_GC_FUNCTION)
        values.function = pGC->alu;

    if (mask & XCB_GC_PLANE_MASK)
        values.plane_mask = pGC->planemask;

    if (mask & XCB_GC_FOREGROUND)
        values.foreground = pGC->fgPixel;

    if (mask & XCB_GC_BACKGROUND)
        values.background = pGC->bgPixel;

    if (mask & XCB_GC_LINE_WIDTH)
        values.line_width = pGC->lineWidth;

    if (mask & XCB_GC_LINE_STYLE)
        values.line_style = pGC->lineStyle;

    if (mask & XCB_GC_CAP_STYLE)
        values.cap_style = pGC->capStyle;

    if (mask & XCB_GC_JOIN_STYLE)
        values.join_style = pGC->joinStyle;

    if (mask & XCB_GC_FILL_STYLE)
        values.fill_style = pGC->fillStyle;

    if (mask & XCB_GC_FILL_RULE)
        values.fill_rule = pGC->fillRule;

    if (mask & XCB_GC_TILE) {
        if (pGC->tileIsPixel)
            mask &= ~GCTile;
        else
            values.tile = XS_PIXMAP_PRIV(pGC->tile.pixmap)->pixmap;
    }

    if (mask & XCB_GC_STIPPLE)
        values.stipple = XS_PIXMAP_PRIV(pGC->stipple)->pixmap;

    if (mask & XCB_GC_TILE_STIPPLE_ORIGIN_X)
        values.tile_stipple_originX = pGC->patOrg.x;

    if (mask & XCB_GC_TILE_STIPPLE_ORIGIN_Y)
        values.tile_stipple_originY = pGC->patOrg.y;

    if (mask & XCB_GC_FONT)
        values.font = XS_FONT_PRIV(pGC->font)->font;

    if (mask & XCB_GC_SUBWINDOW_MODE)
        values.subwindow_mode = pGC->subWindowMode;

    if (mask & XCB_GC_GRAPHICS_EXPOSURES)
        values.graphics_exposures = pGC->graphicsExposures;

    if (mask & XCB_GC_CLIP_ORIGIN_Y)
        values.clip_originX = pGC->clipOrg.x;

    if (mask & XCB_GC_CLIP_ORIGIN_X)
        values.clip_originY = pGC->clipOrg.y;

    if (mask & XCB_GC_CLIP_MASK) /* this is handled in change clip */
        mask &= ~GCClipMask;

    if (mask & XCB_GC_DASH_OFFSET)
        values.dash_offset = pGC->dashOffset;

    if (mask & XCB_GC_DASH_LIST) {
        mask &= ~GCDashList;
        xcb_set_dashes(xsConnection, 
                     XS_GC_PRIV(pGC)->gc, 
                     pGC->dashOffset, 
                     pGC->numInDashList,
                     (uint8_t *)pGC->dash);
    }

    if (mask & XCB_GC_ARC_MODE)
        values.arc_mode = pGC->arcMode;

    if (mask)
        xcb_aux_change_gc(xsConnection, XS_GC_PRIV(pGC)->gc, mask, &values);
}

void xsCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst)
{
    xcb_copy_gc(xsConnection, XS_GC_PRIV(pGCSrc)->gc, XS_GC_PRIV(pGCDst)->gc, mask);
}

void xsDestroyGC(GCPtr pGC)
{
    xcb_free_gc(xsConnection, XS_GC_PRIV(pGC)->gc);
}

void xsChangeClip(GCPtr pGC, int type, pointer pValue, int nRects)
{
    int i, size;
    BoxPtr pBox;
    xcb_rectangle_t *pRects;
    xcb_params_gc_t param;

    xsDestroyClipHelper(pGC);

    switch(type) 
    {
        case CT_NONE:
            param.mask = None;
            xcb_aux_change_gc(xsConnection, XS_GC_PRIV(pGC)->gc, XCB_GC_CLIP_MASK, &param);
            break;

        case CT_REGION:
            nRects = REGION_NUM_RECTS((RegionPtr)pValue);
            size = nRects * sizeof(*pRects);
            pRects = xalloc(size);
            pBox = REGION_RECTS((RegionPtr)pValue);
            for (i = nRects; i-- > 0; ) {
                pRects[i].x = pBox[i].x1;
                pRects[i].y = pBox[i].y1;
                pRects[i].width = pBox[i].x2 - pBox[i].x1;
                pRects[i].height = pBox[i].y2 - pBox[i].y1;
            }
            xcb_set_clip_rectangles(xsConnection, 
                    XCB_CLIP_ORDERING_UNSORTED,
                    XS_GC_PRIV(pGC)->gc, 
                    0, 0,
                    nRects,
                    pRects);
            xfree((char *) pRects);
            break;

        case CT_PIXMAP:
            param.mask = XS_PIXMAP_PRIV((PixmapPtr)pValue)->pixmap;
            xcb_aux_change_gc(xsConnection, XS_GC_PRIV(pGC)->gc, XCB_GC_CLIP_MASK, &param);
            /*
             * Need to change into region, so subsequent uses are with
             * current pixmap contents.
             */
            pGC->clientClip = (pointer) (*pGC->pScreen->BitmapToRegion)((PixmapPtr)pValue);
            (*pGC->pScreen->DestroyPixmap)((PixmapPtr)pValue);
            pValue = pGC->clientClip;
            type = CT_REGION;
            break;

        case CT_UNSORTED:
            xcb_set_clip_rectangles(xsConnection,
                    XCB_CLIP_ORDERING_UNSORTED,
                    XS_GC_PRIV(pGC)->gc, 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    (xcb_rectangle_t *)pValue);
            break;
        case CT_YSORTED:
            xcb_set_clip_rectangles(xsConnection,
                    XCB_CLIP_ORDERING_Y_SORTED,
                    XS_GC_PRIV(pGC)->gc, 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    pValue);
            break;
        case CT_YXSORTED:
            xcb_set_clip_rectangles(xsConnection,
                    XCB_CLIP_ORDERING_YX_SORTED,
                    XS_GC_PRIV(pGC)->gc, 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    pValue);
            break;
        case CT_YXBANDED:
            xcb_set_clip_rectangles(xsConnection,
                    XCB_CLIP_ORDERING_YX_BANDED,
                    XS_GC_PRIV(pGC)->gc, 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    pValue);
            break;
    }

    switch(type) 
    {
        default:
            break;

        case CT_UNSORTED:
        case CT_YSORTED:
        case CT_YXSORTED:
        case CT_YXBANDED:

            /*
             * other parts of server can only deal with CT_NONE,
             * CT_PIXMAP and CT_REGION client clips.
             */
            pGC->clientClip = (pointer) RECTS_TO_REGION(pGC->pScreen, nRects,
                    (xRectangle *)pValue, type);
            xfree(pValue);
            pValue = pGC->clientClip;
            type = CT_REGION;

            break;
    }

    pGC->clientClipType = type;
    pGC->clientClip = pValue;
    //XS_GC_PRIV(pGC)->nClipRects = nRects;
}

void xsDestroyClip(GCPtr pGC)
{
    xcb_params_gc_t param;
    param.mask = None;
    xsDestroyClipHelper(pGC);

    xcb_aux_change_gc(xsConnection, XS_GC_PRIV(pGC)->gc, XCB_GC_CLIP_MASK, &param);


    pGC->clientClipType = CT_NONE;
    pGC->clientClip = NULL;
    //XS_GC_PRIV(pGC)->nClipRects = 0;
}

void xsDestroyClipHelper(GCPtr pGC)
{
    switch (pGC->clientClipType)
    {
        default:
        case CT_NONE:
            break;

        case CT_REGION:
            REGION_DESTROY(pGC->pScreen, pGC->clientClip); 
            break;
    }
}

void xsCopyClip(GCPtr pGCDst, GCPtr pGCSrc)
{
    RegionPtr pRgn;

    switch (pGCSrc->clientClipType)
    {
        default:
        case CT_NONE:
            xsDestroyClip(pGCDst);
            break;

        case CT_REGION:
            pRgn = REGION_CREATE(pGCDst->pScreen, NULL, 1);
            REGION_COPY(pGCDst->pScreen, pRgn, pGCSrc->clientClip);
            xsChangeClip(pGCDst, CT_REGION, pRgn, 0);
            break;
    }
}
