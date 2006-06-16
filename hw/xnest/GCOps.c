/* $Xorg: GCOps.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
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
/* $XFree86: xc/programs/Xserver/hw/xnest/GCOps.c,v 3.5 2003/07/16 01:38:51 dawes Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif
#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_image.h>
#include "regionstr.h"
#include <X11/fonts/fontstruct.h>
#include "gcstruct.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "region.h"
#include "servermd.h"

#include "Xnest.h"

#include "Display.h"
#include "Screen.h"
#include "XNGC.h"
#include "XNFont.h"
#include "GCOps.h"
#include "Drawable.h"
#include "Visual.h"

void xnestFillSpans(DrawablePtr pDrawable, GCPtr pGC, int nSpans, xPoint *pPoints,
        int *pWidths, int fSorted)
{
    ErrorF("xnest warning: function xnestFillSpans not implemented\n");
}

void xnestSetSpans(DrawablePtr pDrawable, GCPtr pGC, char *pSrc,
        xPoint *pPoints, int *pWidths, int nSpans, int fSorted)
{
    ErrorF("xnest warning: function xnestSetSpans not implemented\n");
}

void xnestGetSpans(DrawablePtr pDrawable, int maxWidth, DDXPointPtr pPoints,
        int *pWidths, int nSpans, char *pBuffer)
{
    ErrorF("xnest warning: function xnestGetSpans not implemented\n");
}

void xnestQueryBestSize(int class, unsigned short *pWidth, unsigned short *pHeight,
        ScreenPtr pScreen)
{
    XCBQueryBestSizeCookie c;
    XCBQueryBestSizeRep    *r;


    c = XCBQueryBestSize(xnestConnection, class, (XCBDRAWABLE)xnestDefaultWindows[pScreen->myNum], *pWidth,*pHeight);
    r = XCBQueryBestSizeReply(xnestConnection, c, NULL);

    *pWidth = r->width;
    *pHeight = r->height;
}

void xnestPutImage(DrawablePtr pDrawable, GCPtr pGC, int depth, int x, int y,
        int w, int h, int leftPad, int format, char *pImage)
{
    int size, pad;
    int bpp;
    int i;
    for (i=0; i<xnestNumPixmapFormats; i++) {
        if (xnestPixmapFormats[i].depth == depth) {
            pad = xnestPixmapFormats[i].scanline_pad;
            bpp = xnestPixmapFormats[i].bits_per_pixel;
        }
    }
    size = (((bpp * w + pad - 1) & -pad) >> 3)*h;
    XCBPutImage(xnestConnection,
                format,
                xnestDrawable(pDrawable),
                xnestGC(pGC),
                w, h,
                x, y,
                leftPad,
                depth,
                size,
                (CARD8*) pImage+leftPad);
}

void xnestGetImage(DrawablePtr pDrawable, int x, int y, int w, int h,
        unsigned int format, unsigned long planeMask,
        char *pImage)
{
    XCBImage *img;
    int length;

    img = XCBImageGet(xnestConnection,
                      xnestDrawable(pDrawable),
                      x, y,
                      w, h,
                      planeMask,
                      format);

    if (img) {
        length = img->bytes_per_line * img->height;
        memmove(pImage, img->data, length);
        XCBImageDestroy(img);
    }
}

static Bool xnestBitBlitPredicate(XCBGenericEvent *event)
{
    return (event->response_type == XCBGraphicsExposure || event->response_type == XCBNoExposure);
}

static RegionPtr xnestBitBlitHelper(GCPtr pGC)
{
    int err;
    if (!pGC->graphicsExposures) 
        return NullRegion;
    else {
        XCBGenericEvent *event;
        XCBGraphicsExposureEvent *exp;
        RegionPtr pReg, pTmpReg;
        BoxRec Box;
        Bool pending, overlap;

        pReg = REGION_CREATE(pGC->pScreen, NULL, 1);
        pTmpReg = REGION_CREATE(pGC->pScreen, NULL, 1);
        if(!pReg || !pTmpReg) 
            return NullRegion;

        pending = True;
        while (pending) {
            event = XCBPollForEvent(xnestConnection, &err);
            switch (event->response_type) {
                case XCBNoExposure:
                    pending = False;
                    break;

                case XCBGraphicsExposure:
                    exp = (XCBGraphicsExposureEvent *) event;
                    Box.x1 = exp->x;
                    Box.y1 = exp->y;
                    Box.x2 = exp->x + exp->width;
                    Box.y2 = exp->y + exp->height;
                    REGION_RESET(pGC->pScreen, pTmpReg, &Box);
                    REGION_APPEND(pGC->pScreen, pReg, pTmpReg);
                    pending = exp->count;
                    break;
                default:
                    ErrorF("File: %s Error: %d\n", __FILE__, err);
                    xnestHandleEvent(event);

            }
        }

        REGION_DESTROY(pGC->pScreen, pTmpReg);
        REGION_VALIDATE(pGC->pScreen, pReg, &overlap);
        return(pReg);
    }
}

RegionPtr xnestCopyArea(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
        GCPtr pGC, int srcx, int srcy, int width, int height,
        int dstx, int dsty)
{
    XCBCopyArea(xnestConnection, 
                xnestDrawable(pSrcDrawable),
                xnestDrawable(pDstDrawable),
                xnestGC(pGC),
                srcx, srcy,
                dstx, dsty, 
                width, height);

    return xnestBitBlitHelper(pGC);
}

RegionPtr xnestCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
        GCPtr pGC, int srcx, int srcy, int width, int height,
        int dstx, int dsty, unsigned long plane)
{
    XCBCopyPlane(xnestConnection,
                 xnestDrawable(pSrcDrawable),
                 xnestDrawable(pDstDrawable),
                 xnestGC(pGC),
                 srcx, srcy,
                 dstx, dsty,
                 width, height,
                 plane);

    return xnestBitBlitHelper(pGC);
}

void xnestPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode, int nPoints,
        DDXPointPtr pPoints)
{
    XCBPolyPoint(xnestConnection,
                 mode,
                 xnestDrawable(pDrawable),
                 xnestGC(pGC),
                 nPoints,
                 (XCBPOINT *)pPoints);
}

void xnestPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode, int nPoints,
        DDXPointPtr pPoints)
{
    XCBPolyLine(xnestConnection,
                 mode,
                 xnestDrawable(pDrawable),
                 xnestGC(pGC),
                 nPoints,
                 (XCBPOINT *)pPoints);
}

void xnestPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nSegments,
        xSegment *pSegments)
{
    XCBPolySegment(xnestConnection,
                   xnestDrawable(pDrawable),
                   xnestGC(pGC),
                   nSegments,
                   (XCBSEGMENT *)pSegments);
}

void xnestPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nRectangles,
        xRectangle *pRectangles)
{
    XCBPolyRectangle(xnestConnection,
                     xnestDrawable(pDrawable),
                     xnestGC(pGC),
                     nRectangles,
                     (XCBRECTANGLE *)pRectangles);
}

void xnestPolyArc(DrawablePtr pDrawable, GCPtr pGC, int nArcs, xArc *pArcs)
{
    XCBPolyArc(xnestConnection,
               xnestDrawable(pDrawable),
               xnestGC(pGC),
               nArcs,
               (XCBARC *)pArcs);
}

void xnestFillPolygon(DrawablePtr pDrawable, GCPtr pGC, int shape, int mode,
        int nPoints, DDXPointPtr pPoints)
{
    XCBFillPoly(xnestConnection,
                xnestDrawable(pDrawable),
                xnestGC(pGC), 
                shape,
                mode,
                nPoints,
                (XCBPOINT *)pPoints);
}

void xnestPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nRectangles,
        xRectangle *pRectangles)
{
XCBPolyFillRectangle(xnestConnection,
                     xnestDrawable(pDrawable),
                     xnestGC(pGC),
                     nRectangles,
                     (XCBRECTANGLE*)pRectangles);
}

void xnestPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int nArcs, xArc *pArcs)
{
    XCBPolyFillArc(xnestConnection,
                   xnestDrawable(pDrawable),
                   xnestGC(pGC),
                   nArcs,
                   (XCBARC *)pArcs);
}

int xnestPolyText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, char *string)
{
    int width, i;
    XCBCHAR2B *str;
    XCBFONTABLE f;
    XCBQueryTextExtentsCookie c;
    XCBQueryTextExtentsRep *r;
    XCBGenericError *e;

    XCBPolyText8(xnestConnection,
                 xnestDrawable(pDrawable),
                 xnestGC(pGC),
                 x, y,
                 count,
                 (BYTE *)string);
    
    f.font = xnestFont(pGC->font);
    f.gcontext = xnestGC(pGC);
    str = xalloc(count * sizeof(XCBCHAR2B));
    for (i=0; i<count; i++) {
        str[i].byte1 = string[i];
        str[i].byte2 = '\0';
    }
    c = XCBQueryTextExtents(xnestConnection, f, count, str);
    xfree(str);
    r = XCBQueryTextExtentsReply(xnestConnection, c, NULL);
    if (r)
        if (!e)
            width = r->overall_width;
    /*handle error.. what's appropriate?*/
    return width + x;
}



int xnestPolyText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, unsigned short *string)
{
    int width = 0;
    XCBFONTABLE f;
    XCBQueryTextExtentsCookie c;
    XCBQueryTextExtentsRep *r;
    XCBGenericError *e;

    XCBPolyText16(xnestConnection,
                  xnestDrawable(pDrawable),
                  xnestGC(pGC),
                  x, y,
                  count*2,
                  (BYTE *)string);
    f.font = xnestFont(pGC->font);
    f.gcontext = xnestGC(pGC);
    c = XCBQueryTextExtents(xnestConnection, f, count, (XCBCHAR2B*)string);
    r = XCBQueryTextExtentsReply(xnestConnection, c, &e);
    if (r)
        if (!e)
            width = r->overall_width;
    /*handle error.. what's appropriate?*/
    return width + x;
}

void xnestImageText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count,
        char *string)
{
    XCBImageText8(xnestConnection,
                  count,
                  xnestDrawable(pDrawable),
                  xnestGC(pGC),
                  x, y,
                  string);
}

void xnestImageText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, unsigned short *string)
{
    XCBImageText16(xnestConnection,
                   count,
                   xnestDrawable(pDrawable),
                   xnestGC(pGC),
                   x, y,
                   (XCBCHAR2B *)string);
}

void xnestImageGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
        unsigned int nGlyphs, CharInfoPtr *pCharInfo,
        pointer pGlyphBase)
{
    ErrorF("xnest warning: function xnestImageGlyphBlt not implemented\n");
}

void xnestPolyGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
        unsigned int nGlyphs, CharInfoPtr *pCharInfo,
        pointer pGlyphBase)
{
    ErrorF("xnest warning: function xnestPolyGlyphBlt not implemented\n");
}

void xnestPushPixels(GCPtr pGC, PixmapPtr pBitmap, DrawablePtr pDst,
        int width, int height, int x, int y)
{
    XCBParamsGC param;
    XCBRECTANGLE rect;
    /* only works for solid bitmaps */
    if (pGC->fillStyle == FillSolid)
    {
        param.stipple = xnestPixmap(pBitmap).xid;
        param.tile_stipple_originX = x;
        param.tile_stipple_originY = y;
        param.fill_style = XCBFillStyleStippled;
        XCBAuxChangeGC(xnestConnection, xnestGC(pGC), 
                       XCBGCStipple | XCBGCTileStippleOriginX | XCBGCTileStippleOriginY | XCBGCFillStyle,
                       &param);
        rect.x = x;
        rect.y = y;
        rect.width = width;
        rect.height = height;
        XCBPolyFillRectangle (xnestConnection,
                          xnestDrawable(pDst),
                          xnestGC(pGC),
                          1,
                          &rect);
        param.fill_style = XCBFillStyleSolid;
         XCBAuxChangeGC(xnestConnection, xnestGC(pGC), 
                       XCBGCFillStyle,
                       &param);
    }
    else
        ErrorF("xnest warning: function xnestPushPixels not implemented\n");
}

