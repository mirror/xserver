#ifdef HAVE_XS_CONFIG_H
#include <xs-config.h>
#endif
#include <X11/Xmd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/xcb_image.h>
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
    xcb_query_best_size_cookie_t c;
    xcb_query_best_size_reply_t    *r;


    c = xcb_query_best_size(xsConnection, class, (xcb_drawable_t)xsBackingRoot, *pWidth,*pHeight);
    r = xcb_query_best_size_reply(xsConnection, c, NULL);

    *pWidth = r->width;
    *pHeight = r->height;
}

void xsPutImage(DrawablePtr pDrawable, GCPtr pGC, int depth, int x, int y,
        int w, int h, int leftPad, int format, char *pImage)
{
    int size;
    size = xsPixmapCalcSize(depth, w, h);
    xcb_put_image(xsConnection,
                format,
                XS_DRAWABLE_ID(pDrawable),
                XS_GC_PRIV(pGC)->gc,
                w, h,
                x, y,
                leftPad,
                depth,
                size,
                (uint8_t*) (pImage+leftPad));
}

void xsGetImage(DrawablePtr pDrawable, int x, int y, int w, int h,
        unsigned int format, unsigned long planeMask,
        char *pImage)
{
    xcb_image_t *img;
    int length;

    img = xcb_image_get(xsConnection,
                      XS_DRAWABLE_ID(pDrawable),
                      x, y,
                      w, h,
                      planeMask,
                      format);

    if (img) {
        length = img->bytes_per_line * img->height;
        memmove(pImage, img->data, length);
        xcb_image_destroy(img);
    }
}

static Bool xsBitBlitPredicate(xcb_generic_event_t *event)
{
    return (event->response_type == XCB_GRAPHICS_EXPOSURE || event->response_type == XCB_NO_EXPOSURE);
}

static RegionPtr xsBitBlitHelper(GCPtr pGC)
{
    int err;
    if (!pGC->graphicsExposures) 
        return NullRegion;
    else {
        xcb_generic_event_t *event;
        xcb_graphics_exposure_event_t *exp;
        RegionPtr pReg, pTmpReg;
        BoxRec Box;
        Bool pending, overlap;

        pReg = REGION_CREATE(pGC->pScreen, NULL, 1);
        pTmpReg = REGION_CREATE(pGC->pScreen, NULL, 1);
        if(!pReg || !pTmpReg) 
            return NullRegion;

        pending = TRUE;
        while (pending) {
            event = xcb_poll_for_event(xsConnection);
            if (!event)
                break;
            switch (event->response_type) {
                case XCB_NO_EXPOSURE:
                    pending = FALSE;
                    break;

                case XCB_GRAPHICS_EXPOSURE:
                    exp = (xcb_graphics_exposure_event_t *) event;
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
    xcb_copy_area(xsConnection, 
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
    xcb_copy_plane(xsConnection,
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
    xcb_poly_point(xsConnection,
                 mode,
                 XS_DRAWABLE_ID(pDrawable),
                 XS_GC_PRIV(pGC)->gc,
                 nPoints,
                 (xcb_point_t *)pPoints);
}

void xsPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode, int nPoints,
        DDXPointPtr pPoints)
{
    xcb_poly_line(xsConnection,
                 mode,
                 XS_DRAWABLE_ID(pDrawable),
                 XS_GC_PRIV(pGC)->gc,
                 nPoints,
                 (xcb_point_t *)pPoints);
}

void xsPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nSegments,
        xSegment *pSegments)
{
    xcb_poly_segment(xsConnection,
                   XS_DRAWABLE_ID(pDrawable),
                   XS_GC_PRIV(pGC)->gc,
                   nSegments,
                   (xcb_segment_t *)pSegments);
}

void xsPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nRectangles,
        xRectangle *pRectangles)
{
    xcb_poly_rectangle(xsConnection,
                     XS_DRAWABLE_ID(pDrawable),
                     XS_GC_PRIV(pGC)->gc,
                     nRectangles,
                     (xcb_rectangle_t *)pRectangles);
}

void xsPolyArc(DrawablePtr pDrawable, GCPtr pGC, int nArcs, xArc *pArcs)
{
    xcb_poly_arc(xsConnection,
               XS_DRAWABLE_ID(pDrawable),
               XS_GC_PRIV(pGC)->gc,
               nArcs,
               (xcb_arc_t *)pArcs);
}

void xsFillPolygon(DrawablePtr pDrawable, GCPtr pGC, int shape, int mode,
        int nPoints, DDXPointPtr pPoints)
{
    xcb_fill_poly(xsConnection,
                XS_DRAWABLE_ID(pDrawable),
                XS_GC_PRIV(pGC)->gc, 
                shape,
                mode,
                nPoints,
                (xcb_point_t *)pPoints);
}

void xsPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nRectangles,
        xRectangle *pRectangles)
{
xcb_poly_fill_rectangle(xsConnection,
                     XS_DRAWABLE_ID(pDrawable),
                     XS_GC_PRIV(pGC)->gc,
                     nRectangles,
                     (xcb_rectangle_t*)pRectangles);
}

void xsPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int nArcs, xArc *pArcs)
{
    xcb_poly_fill_arc(xsConnection,
                   XS_DRAWABLE_ID(pDrawable),
                   XS_GC_PRIV(pGC)->gc,
                   nArcs,
                   (xcb_arc_t *)pArcs);
}

int xsPolyText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, char *string)
{
#if 0
    int width, i;
    xcb_char2b_t *str;
    xcb_fontable_t f;
    xcb_query_text_extents_cookie_t c;
    xcb_query_text_extents_reply_t *r;
    xcb_generic_error_t *e;

    xcb_poly_text_8(xsConnection,
                 XS_DRAWABLE_ID(pDrawable),
                 XS_GC_PRIV(pGC)->gc,
                 x, y,
                 count,
                 (uint8_t *)string);
    
    f.font = xsFont(pGC->font);
    f.gcontext = XS_GC_PRIV(pGC)->gc;
    str = xalloc(count * sizeof(xcb_char2b_t));
    for (i=0; i<count; i++) {
        str[i].byte1 = string[i];
        str[i].byte2 = '\0';
    }
    c = xcb_query_text_extents(xsConnection, f, count, str);
    xfree(str);
    r = xcb_query_text_extents_reply(xsConnection, c, NULL);
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
    xcb_fontable_t f;
    xcb_query_text_extents_cookie_t c;
    xcb_query_text_extents_reply_t *r;
    xcb_generic_error_t *e;

    xcb_poly_text_16(xsConnection,
                  XS_DRAWABLE_ID(pDrawable),
                  XS_GC_PRIV(pGC)->gc,
                  x, y,
                  count*2,
                  (uint8_t *)string);
    f.font = xsFont(pGC->font);
    f.gcontext = XS_GC_PRIV(pGC)->gc;
    c = xcb_query_text_extents(xsConnection, f, count, (xcb_char2b_t*)string);
    r = xcb_query_text_extents_reply(xsConnection, c, &e);
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
    xcb_image_text_8(xsConnection,
                  count,
                  XS_DRAWABLE_ID(pDrawable),
                  XS_GC_PRIV(pGC)->gc,
                  x, y,
                  string);
}

void xsImageText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, unsigned short *string)
{
    xcb_image_text_16(xsConnection,
                   count,
                   XS_DRAWABLE_ID(pDrawable),
                   XS_GC_PRIV(pGC)->gc,
                   x, y,
                   (xcb_char2b_t *)string);
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
    xcb_params_gc_t param;
    xcb_rectangle_t rect;
    /* only works for solid bitmaps */
    if (pGC->fillStyle == FillSolid)
    {
        param.stipple = XS_PIXMAP_PRIV(pBitmap)->pixmap;
        param.tile_stipple_originX = x;
        param.tile_stipple_originY = y;
        param.fill_style = XCB_FILL_STYLE_STIPPLED;
        xcb_aux_change_gc(xsConnection, XS_GC_PRIV(pGC)->gc, 
                       XCB_GC_STIPPLE | XCB_GC_TILE_STIPPLE_ORIGIN_X | XCB_GC_TILE_STIPPLE_ORIGIN_Y | XCB_GC_FILL_STYLE,
                       &param);
        rect.x = x;
        rect.y = y;
        rect.width = width;
        rect.height = height;
        xcb_poly_fill_rectangle (xsConnection,
                          XS_DRAWABLE_ID(pDst),
                          XS_GC_PRIV(pGC)->gc,
                          1,
                          &rect);
        param.fill_style = XCB_FILL_STYLE_SOLID;
         xcb_aux_change_gc(xsConnection, XS_GC_PRIV(pGC)->gc, 
                       XCB_GC_FILL_STYLE,
                       &param);
    }
    else
        ErrorF("xs warning: function xsPushPixels not implemented\n");
}

