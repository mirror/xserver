/* $Xorg: Display.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
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
/* $XFree86: xc/programs/Xserver/hw/xnest/Display.c,v 3.4 2001/10/28 03:34:10 tsi Exp $ */


#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xproto.h>

#include "screenint.h"
#include "input.h"
#include "misc.h"
#include "scrnintstr.h"
#include "servermd.h"

#include "Xnest.h"

#include "Display.h"
#include "Init.h"
#include "Args.h"

#include "icon"
#include "screensaver"
XCBConnection  *xnestConnection = NULL;
XCBVISUALTYPE **xnestVisuals = NULL;
int             xnestNumVisuals;
CARD8          *xnestDepthForVisual = NULL;
int             xnestDefaultVisualIndex;
XCBCOLORMAP    *xnestDefaultColormaps = NULL;
int             xnestNumDefaultColormaps;
CARD8          *xnestDepths = NULL;
int             xnestNumDepth;
XCBFORMAT      *xnestPixmapFormats = NULL;
int             xnestNumPixmapFormats;
CARD32          xnestBlackPixel;
CARD32          xnestWhitePixel;
XCBDRAWABLE     xnestDefaultDrawables[MAXDEPTH + 1];
XCBPIXMAP       xnestIconBitmap;
XCBPIXMAP       xnestScreenSaverPixmap;
XCBGCONTEXT     xnestBitmapGC;
unsigned long   xnestEventMask;


static XCBVISUALTYPE **xnestListVisuals(XCBConnection *c, CARD8 **depths, int *numvisuals)
{
    XCBVISUALTYPEIter viter;
    XCBDEPTHIter      diter;
    XCBSCREEN        *screen;
    XCBVISUALTYPE   **vis = NULL;
    /*reserve for NULL termination*/
    int               nvis = 1;
    int               i = 0;

    screen = XCBSetupRootsIter(XCBGetSetup(c)).data;
    diter = XCBSCREENAllowedDepthsIter(screen);
    for (; diter.rem; XCBDEPTHNext(&diter)) {
        viter = XCBDEPTHVisualsIter (diter.data);
        nvis += viter.rem;
        vis = xrealloc(vis, sizeof(XCBVISUALTYPE) * (nvis+1));
        *depths = xrealloc(*depths, sizeof(CARD8) * (nvis+1));
        vis[nvis-1] = NULL;
        for ( ; viter.rem; XCBVISUALTYPENext(&viter)){
            (*depths)[i] = diter.data->depth;
            vis[i++] = viter.data;
        }
    }
    *numvisuals = nvis-1;
    return vis;
}

static CARD8 *xnestListDepths(XCBConnection *c, int *numdepths)
{
    XCBDEPTHIter diter;
    CARD8       *depths = NULL;
    XCBSCREEN   *screen;
    int          ndepth = 0;
    int          i=0;

    screen = XCBSetupRootsIter(XCBGetSetup(c)).data;
    diter = XCBSCREENAllowedDepthsIter(screen);

    ndepth = diter.rem;
    depths = xalloc(sizeof(CARD8)*(ndepth+1));
    for (; diter.rem; XCBDEPTHNext(&diter)) {
        depths[i++]=diter.data->depth;
    }
    *numdepths = ndepth-1;
    return depths;
}

void xnestOpenDisplay(int argc, char *argv[])
{
    int screennum;
    XCBSCREEN *screen;
    XCBVISUALID vid = {None};
    XCBDRAWABLE root;
    int i, j;
    char *display;

    if (!xnestDoFullGeneration) return;
    xnestCloseDisplay();

    /* try connecting to the server. */
    xnestConnection = XCBConnect(xnestDisplayName, &screennum);
    if (xnestConnection == NULL) { /* failure to connect */
        /* prettify the display name */
        display = xnestDisplayName;
        if (!display || !*display) {
            display = getenv("DISPLAY");
            if (!display)
                display = "";
        }
        FatalError("Unable to open display \"%s\".\n", display);
    }
    
    xnestDepths  = xnestListDepths(xnestConnection, &xnestNumDepth);
    xnestVisuals = xnestListVisuals(xnestConnection, &xnestDepthForVisual, &xnestNumVisuals);
    screen = XCBSetupRootsIter (XCBGetSetup (xnestConnection)).data;
    root = (XCBDRAWABLE)XCBSetupRootsIter(XCBGetSetup(xnestConnection)).data->root;
    xnestDefaultDepth = screen->root_depth;
    
    if (!xnestVisuals)
        FatalError("Unable to find any visuals");
    if (xnestUserDefaultClass || xnestUserDefaultDepth) {
        for (i=0; i < xnestNumVisuals; i++) {
            if ((!xnestUserDefaultClass || xnestVisuals[i]->_class == xnestDefaultClass) &&
                (!xnestUserDefaultDepth || xnestDepthForVisual[i] == xnestDefaultDepth)) {
                  xnestDefaultVisualIndex = i;
            }
        }
    } else {
        screen = XCBSetupRootsIter (XCBGetSetup (xnestConnection)).data;
        vid = screen->root_visual;
        for (i=0; i<xnestNumVisuals ; i++) {
            if ( vid.id == xnestVisuals[i]->visual_id.id) {
                xnestDefaultVisualIndex = i;
                break;
            }
        }
    }

    xnestNumDefaultColormaps = xnestNumVisuals;
    xnestDefaultColormaps = xalloc(xnestNumDefaultColormaps * sizeof(XCBCOLORMAP));
    for (i=0; i<xnestNumDefaultColormaps; i++) {
        xnestDefaultColormaps[i] = XCBCOLORMAPNew(xnestConnection);
        XCBCreateColormap(xnestConnection,      /*connection*/
                          AllocNone,
                          xnestDefaultColormaps[i],
                          screen->root,
                          xnestVisuals[i]->visual_id);
    }

    xnestPixmapFormats = XCBSetupPixmapFormats(XCBGetSetup(xnestConnection));
    xnestNumPixmapFormats = XCBSetupPixmapFormatsLength(XCBGetSetup(xnestConnection));
    
    xnestBlackPixel = screen->black_pixel;
    xnestWhitePixel = screen->white_pixel;

    if (xnestParentWindow.xid != 0)
        xnestEventMask = XCBEventMaskStructureNotify;
    else
        xnestEventMask = 0L;

    for (i=0; i<= MAXDEPTH; i++)
        xnestDefaultDrawables[i].pixmap.xid = None;

    for (i=0; i <= MAXDEPTH; i++)
        xnestDefaultDrawables[i].pixmap.xid = None;

    for (i=0; i < xnestNumPixmapFormats; i++) {
        for (j=0; j < xnestNumDepth; j++) {
            if ((xnestPixmapFormats[i].depth == xnestDepths[j])||
                (xnestPixmapFormats[i].depth == 1 )) {
                xnestDefaultDrawables[xnestPixmapFormats[i].depth].pixmap = XCBPIXMAPNew(xnestConnection);
                XCBCreatePixmap(xnestConnection,
                                xnestPixmapFormats[i].depth,
                                xnestDefaultDrawables[xnestPixmapFormats[i].depth].pixmap,
                                root,
                                1, 1);
            }
        }
    }

    xnestBitmapGC = XCBGCONTEXTNew(xnestConnection);
    XCBCreateGC(xnestConnection,
                xnestBitmapGC,
                root,
                0,
                NULL);

    if (!(xnestUserGeometry & XValue))
        xnestX = 0;
    if (!(xnestUserGeometry & YValue))
        xnestY = 0;

    if (xnestParentWindow.xid  == 0) {
        if (!(xnestUserGeometry & WidthValue))
            xnestWidth = 3 * screen->width_in_pixels / 4;

        if (!(xnestUserGeometry & HeightValue))
            xnestHeight = 3 * screen->height_in_pixels / 4;
    }

    if (!xnestUserBorderWidth)
        xnestBorderWidth = 1;
    xnestIconBitmap = XCBPIXMAPNew(xnestConnection);
    XCBCreatePixmap(xnestConnection, /*connection*/
                    xnestDefaultDepth,/*depth*/
                    xnestIconBitmap, /*pixmap*/
                    root,            /*drawable*/
                    icon_width,icon_height); /*width,height*/
    XCBPutImage(xnestConnection,        /*connection*/
                XCBImageFormatXYBitmap, /*format*/
                (XCBDRAWABLE)xnestIconBitmap,        /*drawable*/
                xnestBitmapGC,          /*gc*/
                icon_width,icon_height, /*width, height*/
                0,0,                    /*dst_x, dst_y*/
                0,                      /*pad*/
                1,                      /*depth*/
                sizeof(icon_bits),      /*length..correct??*/
                icon_bits);             /*bits*/

    xnestScreenSaverPixmap = XCBPIXMAPNew(xnestConnection);
    XCBCreatePixmap(xnestConnection, /*connection*/
                    xnestDefaultDepth, /*depth*/
                    xnestScreenSaverPixmap, /*pixmap*/
                    root,            /*drawable*/
                    icon_width,icon_height); /*width,height*/
    XCBPutImage(xnestConnection,        /*connection*/
                XCBImageFormatXYBitmap, /*format*/
                (XCBDRAWABLE)xnestScreenSaverPixmap,/*drawable*/
                xnestBitmapGC,          /*gc*/
                icon_width,icon_height, /*width, height*/
                0,0,                    /*dst_x, dst_y*/
                0,                      /*pad*/
                1,                      /*depth*/
                sizeof(icon_bits),      /*length..correct??*/
                icon_bits);             /*bits*/
               
}

void xnestCloseDisplay()
{
    if (!xnestDoFullGeneration || !xnestConnection) return;

    /*
       If xnestDoFullGeneration all x resources will be destroyed upon closing
       the display connection.  There is no need to generate extra protocol.
       */

    xfree(xnestDefaultColormaps);
    XFree(xnestVisuals);
    XFree(xnestDepths);
    /*XFree(xnestPixmapFormats); freeing this crashes Xnest...*/
    XCBDisconnect(xnestConnection);
}
