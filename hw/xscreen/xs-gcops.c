#ifdef HAVE_XNEST_CONFIG_H
#include <xs-config.h>
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


#include "xs-globals.h"
#include "xs-pixmap.h"
#include "xs-window.h"
#include "xs-gcops.h"
#include "xs-gc.h"

void xsFillSpans(DrawablePtr pDrawable, GCPtr pGC, int nSpans, xPoint *pPoints,
        int *pWidths, int fSorted)
{
    ErrorF("xs warning: function xsFillSpans not implemented\n");
}

void xsSetSpans(DrawablePtr pDrawable, GCPtr pGC, char *pSrc,
        xPoint *pPoints, int *pWidths, int nSpans, int fSorted)
{
    ErrorF("xs warning: function xsSetSpans not implemented\n");
}

void xsGetSpans(DrawablePtr pDrawable, int maxWidth, DDXPointPtr pPoints,
        int *pWidths, int nSpans, char *pBuffer)
{
    ErrorF("xs warning: function xsGetSpans not implemented\n");
}

void xsQueryBestSize(int class, unsigned short *pWidth, unsigned short *pHeight,
        ScreenPtr pScreen)
{
    XCBQueryBestSizeCookie c;
    XCBQueryBestSizeRep    *r;


    c = XCBQueryBestSize(xsConnection, class, (XCBDRAWABLE)xsDefaultWindow, *pWidth,*pHeight);
    r = XCBQueryBestSizeReply(xsConnection, c, NULL);

    *pWidth = r->width;
    *pHeight = r->height;
}

void xsPutImage(DrawablePtr pDrawable, GCPtr pGC, int depth, int x, int y,
        int w, int h, int leftPad, int format, char *pImage)
{
    int size;
    int i;
    size = xsPixmapCalcSize(depth, w, h);
    XCBPutImage(xsConnection,
                format,
                XS_DRAWABLE_ID(pDrawable),
                XS_GC_PRIV(pGC)->gc,
                w, h,
                x, y,
                leftPad,
                depth,
                size,
                (CARD8*) (pImage+leftPad));
}

void xsGetImage(DrawablePtr pDrawable, int x, int y, int w, int h,
        unsigned int format, unsigned long planeMask,
        char *pImage)
{
    XCBImage *img;
    int length;

    img = XCBImageGet(xsConnection,
                      XS_DRAWABLE_ID(pDrawable),
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

static Bool xsBitBlitPredicate(XCBGenericEvent *event)
{
    return (event->response_type == XCBGraphicsExposure || event->response_type == XCBNoExposure);
}

static RegionPtr xsBitBlitHelper(GCPtr pGC)
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

        pending = TRUE;
        while (pending) {
            event = XCBPollForEvent(xsConnection, &err);
            if (!event)
                break;
            switch (event->response_type) {
                case XCBNoExposure:
                    pending = FALSE;
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
                    xsHandleEvent(event);

            }
        }

        REGION_DESTROY(pGC->pScreen, pTmpReg);
        REGION_VALIDATE(pGC->pScreen, pReg, &overlap);
        return(pReg);
    }
}

RegionPtr xsCopyArea(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
        GCPtr pGC, int srcx, int srcy, int width, int height,
        int dstx, int dsty)
{
    XCBCopyArea(xsConnection, 
                XS_DRAWABLE_ID(pSrcDrawable),
                XS_DRAWABLE_ID(pDstDrawable),
                XS_GC_PRIV(pGC)->gc,
                srcx, srcy,
                dstx, dsty, 
                width, height);

    return xsBitBlitHelper(pGC);
}

RegionPtr xsCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
        GCPtr pGC, int srcx, int srcy, int width, int height,
        int dstx, int dsty, unsigned long plane)
{
    XCBCopyPlane(xsConnection,
                 XS_DRAWABLE_ID(pSrcDrawable),
                 XS_DRAWABLE_ID(pDstDrawable),
                 XS_GC_PRIV(pGC)->gc,
                 srcx, srcy,
                 dstx, dsty,
                 width, height,
                 plane);

    return xsBitBlitHelper(pGC);
}

void xsPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode, int nPoints,
        DDXPointPtr pPoints)
{
    XCBPolyPoint(xsConnection,
                 mode,
                 XS_DRAWABLE_ID(pDrawable),
                 XS_GC_PRIV(pGC)->gc,
                 nPoints,
                 (XCBPOINT *)pPoints);
}

void xsPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode, int nPoints,
        DDXPointPtr pPoints)
{
    XCBPolyLine(xsConnection,
                 mode,
                 XS_DRAWABLE_ID(pDrawable),
                 XS_GC_PRIV(pGC)->gc,
                 nPoints,
                 (XCBPOINT *)pPoints);
}

void xsPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nSegments,
        xSegment *pSegments)
{
    XCBPolySegment(xsConnection,
                   XS_DRAWABLE_ID(pDrawable),
                   XS_GC_PRIV(pGC)->gc,
                   nSegments,
                   (XCBSEGMENT *)pSegments);
}

void xsPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nRectangles,
        xRectangle *pRectangles)
{
    XCBPolyRectangle(xsConnection,
                     XS_DRAWABLE_ID(pDrawable),
                     XS_GC_PRIV(pGC)->gc,
                     nRectangles,
                     (XCBRECTANGLE *)pRectangles);
}

void xsPolyArc(DrawablePtr pDrawable, GCPtr pGC, int nArcs, xArc *pArcs)
{
    XCBPolyArc(xsConnection,
               XS_DRAWABLE_ID(pDrawable),
               XS_GC_PRIV(pGC)->gc,
               nArcs,
               (XCBARC *)pArcs);
}

void xsFillPolygon(DrawablePtr pDrawable, GCPtr pGC, int shape, int mode,
        int nPoints, DDXPointPtr pPoints)
{
    XCBFillPoly(xsConnection,
                XS_DRAWABLE_ID(pDrawable),
                XS_GC_PRIV(pGC)->gc, 
                shape,
                mode,
                nPoints,
                (XCBPOINT *)pPoints);
}

void xsPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nRectangles,
        xRectangle *pRectangles)
{
XCBPolyFillRectangle(xsConnection,
                     XS_DRAWABLE_ID(pDrawable),
                     XS_GC_PRIV(pGC)->gc,
                     nRectangles,
                     (XCBRECTANGLE*)pRectangles);
}

void xsPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int nArcs, xArc *pArcs)
{
    XCBPolyFillArc(xsConnection,
                   XS_DRAWABLE_ID(pDrawable),
                   XS_GC_PRIV(pGC)->gc,
                   nArcs,
                   (XCBARC *)pArcs);
}

int xsPolyText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, char *string)
{
#if 0
    int width, i;
    XCBCHAR2B *str;
    XCBFONTABLE f;
    XCBQueryTextExtentsCookie c;
    XCBQueryTextExtentsRep *r;
    XCBGenericError *e;

    XCBPolyText8(xsConnection,
                 XS_DRAWABLE_ID(pDrawable),
                 XS_GC_PRIV(pGC)->gc,
                 x, y,
                 count,
                 (BYTE *)string);
    
    f.font = xsFont(pGC->font);
    f.gcontext = XS_GC_PRIV(pGC)->gc;
    str = xalloc(count * sizeof(XCBCHAR2B));
    for (i=0; i<count; i++) {
        str[i].byte1 = string[i];
        str[i].byte2 = '\0';
    }
    c = XCBQueryTextExtents(xsConnection, f, count, str);
    xfree(str);
    r = XCBQueryTextExtentsReply(xsConnection, c, NULL);
    if (r)
        if (!e)
            width = r->overall_width;
    /*handle error.. what's appropriate?*/
    return width + x;
#endif
    ErrorF("Would have printed %s\n", string);
}



int xsPolyText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, unsigned short *string)
{
#if 0
    int width = 0;
    XCBFONTABLE f;
    XCBQueryTextExtentsCookie c;
    XCBQueryTextExtentsRep *r;
    XCBGenericError *e;

    XCBPolyText16(xsConnection,
                  XS_DRAWABLE_ID(pDrawable),
                  XS_GC_PRIV(pGC)->gc,
                  x, y,
                  count*2,
                  (BYTE *)string);
    f.font = xsFont(pGC->font);
    f.gcontext = XS_GC_PRIV(pGC)->gc;
    c = XCBQueryTextExtents(xsConnection, f, count, (XCBCHAR2B*)string);
    r = XCBQueryTextExtentsReply(xsConnection, c, &e);
    if (r)
        if (!e)
            width = r->overall_width;
    /*handle error.. what's appropriate?*/
    return width + x;
#endif
    ErrorF("Would have printed %s\n", string);
}

void xsImageText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count,
        char *string)
{
    XCBImageText8(xsConnection,
                  count,
                  XS_DRAWABLE_ID(pDrawable),
                  XS_GC_PRIV(pGC)->gc,
                  x, y,
                  string);
}

void xsImageText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, unsigned short *string)
{
    XCBImageText16(xsConnection,
                   count,
                   XS_DRAWABLE_ID(pDrawable),
                   XS_GC_PRIV(pGC)->gc,
                   x, y,
                   (XCBCHAR2B *)string);
}

void xsImageGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
        unsigned int nGlyphs, CharInfoPtr *pCharInfo,
        pointer pGlyphBase)
{
    ErrorF("xs warning: function xsImageGlyphBlt not implemented\n");
}

void xsPolyGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
        unsigned int nGlyphs, CharInfoPtr *pCharInfo,
        pointer pGlyphBase)
{
    ErrorF("xs warning: function xsPolyGlyphBlt not implemented\n");
}

void xsPushPixels(GCPtr pGC, PixmapPtr pBitmap, DrawablePtr pDst,
        int width, int height, int x, int y)
{
    XCBParamsGC param;
    XCBRECTANGLE rect;
    /* only works for solid bitmaps */
    if (pGC->fillStyle == FillSolid)
    {
        param.stipple = XS_PIXMAP_PRIV(pBitmap)->pixmap.xid;
        param.tile_stipple_originX = x;
        param.tile_stipple_originY = y;
        param.fill_style = XCBFillStyleStippled;
        XCBAuxChangeGC(xsConnection, XS_GC_PRIV(pGC)->gc, 
                       XCBGCStipple | XCBGCTileStippleOriginX | XCBGCTileStippleOriginY | XCBGCFillStyle,
                       &param);
        rect.x = x;
        rect.y = y;
        rect.width = width;
        rect.height = height;
        XCBPolyFillRectangle (xsConnection,
                          XS_DRAWABLE_ID(pDst),
                          XS_GC_PRIV(pGC)->gc,
                          1,
                          &rect);
        param.fill_style = XCBFillStyleSolid;
         XCBAuxChangeGC(xsConnection, XS_GC_PRIV(pGC)->gc, 
                       XCBGCFillStyle,
                       &param);
    }
    else
        ErrorF("xs warning: function xsPushPixels not implemented\n");
}

