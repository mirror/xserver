/* $Xorg: Screen.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
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
/* $XFree86: xc/programs/Xserver/hw/xnest/Screen.c,v 3.12 2003/11/14 22:25:59 dawes Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_icccm.h>
#include "scrnintstr.h"
#include "dix.h"
#include "mi.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "resource.h"

#include "Xnest.h"

#include "Display.h"
#include "Screen.h"
#include "XNGC.h"
#include "GCOps.h"
#include "Drawable.h"
#include "XNFont.h"
#include "Color.h"
#include "XNCursor.h"
#include "Visual.h"
#include "Events.h"
#include "Init.h"
#include "mipointer.h"
#include "Args.h"

XCBWINDOW xnestDefaultWindows[MAXSCREENS];
XCBWINDOW xnestScreenSaverWindows[MAXSCREENS];

#ifdef GLXEXT
extern void GlxWrapInitVisuals(miInitVisualsProcPtr *);
#endif

#ifdef PIXPRIV
int xnestScreenGeneration = -1;
#endif

ScreenPtr xnestScreen(XCBWINDOW window)
{
    int i;

    for (i = 0; i < xnestNumScreens; i++)
        if (xnestDefaultWindows[i].xid == window.xid) 
            return screenInfo.screens[i];

    return NULL;
}

static int offset(unsigned long mask)
{
    int count;

    for (count = 0; !(mask & 1) && count < 32; count++)
        mask >>= 1;

    return count;
}

static Bool xnestSaveScreen(ScreenPtr pScreen, int what)
{
    if (xnestSoftwareScreenSaver)
        return False;
    else {
        switch (what) {
            case SCREEN_SAVER_ON:
                XCBCirculateWindow(xnestConnection, 
                        XCBCirculateRaiseLowest,
                        xnestScreenSaverWindows[pScreen->myNum]);
                XCBMapWindow(xnestConnection, xnestScreenSaverWindows[pScreen->myNum]);
                xnestSetScreenSaverColormapWindow(pScreen);
                break;

            case SCREEN_SAVER_OFF:
                XCBUnmapWindow(xnestConnection, xnestScreenSaverWindows[pScreen->myNum]);
                xnestSetInstalledColormapWindows(pScreen);
                break;

            case SCREEN_SAVER_FORCER:
                lastEventTime = GetTimeInMillis();
                XCBUnmapWindow(xnestConnection, xnestScreenSaverWindows[pScreen->myNum]);
                xnestSetInstalledColormapWindows(pScreen);
                break;

            case SCREEN_SAVER_CYCLE:
                XCBUnmapWindow(xnestConnection, xnestScreenSaverWindows[pScreen->myNum]);
                xnestSetInstalledColormapWindows(pScreen);
                break;
        }
        return True;
    }
}

static Bool xnestCursorOffScreen(ScreenPtr *ppScreen, int *x, int *y)
{
    return FALSE;
}

static void xnestCrossScreen(ScreenPtr pScreen, Bool entering)
{
}

static miPointerScreenFuncRec xnestPointerCursorFuncs =
{
    xnestCursorOffScreen,
    xnestCrossScreen,
    miPointerWarpCursor
};

static miPointerSpriteFuncRec xnestPointerSpriteFuncs = 
{
    xnestRealizeCursor,
    xnestUnrealizeCursor,
    xnestSetCursor,
    xnestMoveCursor,
};

Bool xnestOpenScreen(int index, ScreenPtr pScreen, int argc, char *argv[])
{
    VisualPtr           visuals;
    DepthPtr            depths;
    int numVisuals,     numDepths;
    int                 i, j, depthIndex;
    unsigned long       valuemask = 0;
    XCBGetGeometryRep  *rattr;
    XCBGetGeometryCookie cattr;
    XCBParamsCW         param;
    SizeHints          *sizeHints;
    VisualID            defaultVisual;
    int                 rootDepth;
    XCBSCREEN          *screen;
    XCBVISUALID         vid;

    if (!(AllocateWindowPrivate(pScreen, xnestWindowPrivateIndex,
                    sizeof(xnestPrivWin))  &&
                AllocateGCPrivate(pScreen, xnestGCPrivateIndex, 
                    sizeof(xnestPrivGC)))) 
        return False;

#ifdef PIXPRIV
    if (xnestScreenGeneration != serverGeneration) {
        if ((xnestPixmapPrivateIndex = AllocatePixmapPrivateIndex()) < 0)
            return False;
        xnestScreenGeneration = serverGeneration;
    }

    if (!AllocatePixmapPrivate(pScreen,xnestPixmapPrivateIndex,
                sizeof (xnestPrivPixmap)))
        return False;
#endif
    visuals = (VisualPtr)xalloc(xnestNumVisuals * sizeof(VisualRec));
    numVisuals = 0;

    depths = (DepthPtr)xalloc(MAXDEPTH * sizeof(DepthRec));
    depths[0].depth = 1;
    depths[0].numVids = 0;
    depths[0].vids = (VisualID *)xalloc(MAXVISUALSPERDEPTH * sizeof(VisualID));
    numDepths = 1;

    for (i = 0; i < xnestNumVisuals; i++) {
        visuals[numVisuals].class = xnestVisuals[i]->_class;
        visuals[numVisuals].bitsPerRGBValue = xnestVisuals[i]->bits_per_rgb_value;
        visuals[numVisuals].ColormapEntries = xnestVisuals[i]->colormap_entries;
        visuals[numVisuals].nplanes = xnestDepthForVisual[i];
        visuals[numVisuals].redMask = xnestVisuals[i]->red_mask;
        visuals[numVisuals].greenMask = xnestVisuals[i]->green_mask;
        visuals[numVisuals].blueMask = xnestVisuals[i]->blue_mask;
        visuals[numVisuals].offsetRed = offset(xnestVisuals[i]->red_mask);
        visuals[numVisuals].offsetGreen = offset(xnestVisuals[i]->green_mask);
        visuals[numVisuals].offsetBlue = offset(xnestVisuals[i]->blue_mask);

        /* Check for and remove duplicates. */
        for (j = 0; j < numVisuals; j++) {
            if (visuals[numVisuals].class           == visuals[j].class           &&
                    visuals[numVisuals].bitsPerRGBValue == visuals[j].bitsPerRGBValue &&
                    visuals[numVisuals].ColormapEntries == visuals[j].ColormapEntries &&
                    visuals[numVisuals].nplanes         == visuals[j].nplanes         &&
                    visuals[numVisuals].redMask         == visuals[j].redMask         &&
                    visuals[numVisuals].greenMask       == visuals[j].greenMask       &&
                    visuals[numVisuals].blueMask        == visuals[j].blueMask        &&
                    visuals[numVisuals].offsetRed       == visuals[j].offsetRed       &&
                    visuals[numVisuals].offsetGreen     == visuals[j].offsetGreen     &&
                    visuals[numVisuals].offsetBlue      == visuals[j].offsetBlue)
                break;
        }
        if (j < numVisuals)
            break;

        visuals[numVisuals].vid = FakeClientID(0);

        depthIndex = UNDEFINED;
        for (j = 0; j < numDepths; j++)
            if (depths[j].depth == xnestDepthForVisual[i]) {
                depthIndex = j;
                break;
            }

        if (depthIndex == UNDEFINED) {
            depthIndex = numDepths;
            depths[depthIndex].depth = xnestDepthForVisual[i];
            depths[depthIndex].numVids = 0;
            depths[depthIndex].vids = 
                (VisualID *)xalloc(MAXVISUALSPERDEPTH * sizeof(VisualID));
            numDepths++;
        }
        if (depths[depthIndex].numVids >= MAXVISUALSPERDEPTH) {
            FatalError("Visual table overflow");
        }
        depths[depthIndex].vids[depths[depthIndex].numVids] = 
            visuals[numVisuals].vid;
        depths[depthIndex].numVids++;

        numVisuals++;
    }
    visuals = (VisualPtr)xrealloc(visuals, numVisuals * sizeof(VisualRec));

    defaultVisual = visuals[xnestDefaultVisualIndex].vid;
    rootDepth = visuals[xnestDefaultVisualIndex].nplanes;

#ifdef GLXEXT
    {
        miInitVisualsProcPtr proc = NULL;

        GlxWrapInitVisuals(&proc);
        /* GlxInitVisuals ignores the last three arguments. */
        proc(&visuals, &depths, &numVisuals, &numDepths,
                &rootDepth, &defaultVisual, 0, 0, 0);
    }
#endif

    if (xnestParentWindow.xid != 0) {
        
        cattr = XCBGetGeometry(xnestConnection, (XCBDRAWABLE)xnestParentWindow);
        rattr = XCBGetGeometryReply(xnestConnection, cattr, NULL);
        xnestWidth = rattr->width;
        xnestHeight = rattr->height;
        free(rattr);
    }

    /* myNum */
    /* id */
    miScreenInit(pScreen, NULL, xnestWidth, xnestHeight, 1, 1, xnestWidth,
            rootDepth,
            numDepths, depths,
            defaultVisual, /* root visual */
            numVisuals, visuals);

    /*  miInitializeBackingStore(pScreen); */

    pScreen->defColormap = (Colormap) FakeClientID(0);
    pScreen->minInstalledCmaps = MINCMAPS;
    pScreen->maxInstalledCmaps = MAXCMAPS;
    pScreen->backingStoreSupport = NotUseful;
    pScreen->saveUnderSupport = NotUseful;
    pScreen->whitePixel = xnestWhitePixel;
    pScreen->blackPixel = xnestBlackPixel;
    /* rgf */
    /* GCperDepth */
    /* PixmapPerDepth */
    pScreen->devPrivate = NULL;
    /* WindowPrivateLen */
    /* WindowPrivateSizes */
    /* totalWindowSize */
    /* GCPrivateLen */
    /* GCPrivateSizes */
    /* totalGCSize */

    /* Random screen procedures */

    pScreen->QueryBestSize = xnestQueryBestSize;
    pScreen->SaveScreen = xnestSaveScreen;
    pScreen->GetImage = xnestGetImage;
    pScreen->GetSpans = xnestGetSpans;
    pScreen->PointerNonInterestBox = NULL;
    pScreen->SourceValidate = NULL;

    /* Window Procedures */

    pScreen->CreateWindow = xnestCreateWindow;
    pScreen->DestroyWindow = xnestDestroyWindow;
    pScreen->PositionWindow = xnestPositionWindow;
    pScreen->ChangeWindowAttributes = xnestChangeWindowAttributes;
    pScreen->RealizeWindow = xnestRealizeWindow;
    pScreen->UnrealizeWindow = xnestUnrealizeWindow;
    pScreen->PostValidateTree = NULL;
    pScreen->WindowExposures = xnestWindowExposures;
    pScreen->PaintWindowBackground = xnestPaintWindowBackground;
    pScreen->PaintWindowBorder = xnestPaintWindowBorder;
    pScreen->CopyWindow = xnestCopyWindow;
    pScreen->ClipNotify = xnestClipNotify;

    /* Pixmap procedures */

    pScreen->CreatePixmap = xnestCreatePixmap;
    pScreen->DestroyPixmap = xnestDestroyPixmap;

    /* Backing store procedures */

    pScreen->SaveDoomedAreas = NULL;
    pScreen->RestoreAreas = NULL;
    pScreen->ExposeCopy = NULL;
    pScreen->TranslateBackingStore = NULL;
    pScreen->ClearBackingStore = NULL;
    pScreen->DrawGuarantee = NULL;

    /* Font procedures */

    pScreen->RealizeFont = xnestRealizeFont;
    pScreen->UnrealizeFont = xnestUnrealizeFont;

    /* GC procedures */

    pScreen->CreateGC = xnestCreateGC;

    /* Colormap procedures */

    pScreen->CreateColormap = xnestCreateColormap;
    pScreen->DestroyColormap = xnestDestroyColormap;
    pScreen->InstallColormap = xnestInstallColormap;
    pScreen->UninstallColormap = xnestUninstallColormap;
    pScreen->ListInstalledColormaps = xnestListInstalledColormaps;
    pScreen->StoreColors = xnestStoreColors;
    pScreen->ResolveColor = xnestResolveColor;

    pScreen->BitmapToRegion = xnestPixmapToRegion;

    /* OS layer procedures */

    pScreen->BlockHandler = (ScreenBlockHandlerProcPtr)NoopDDA;
    pScreen->WakeupHandler = (ScreenWakeupHandlerProcPtr)NoopDDA;
    pScreen->blockData = NULL;
    pScreen->wakeupData = NULL;

    miPointerInitialize (pScreen, &xnestPointerSpriteFuncs, 
            &xnestPointerCursorFuncs, True);

    screen = XCBSetupRootsIter (XCBGetSetup (xnestConnection)).data;
    pScreen->mmWidth = xnestWidth * screen->width_in_millimeters / screen->width_in_pixels;
    pScreen->mmHeight = xnestHeight * screen->height_in_millimeters / screen->height_in_pixels;

    /* overwrite miCloseScreen with our own */
    pScreen->CloseScreen = xnestCloseScreen;

    if (!miScreenDevPrivateInit(pScreen, xnestWidth, NULL))
        return FALSE;

#ifdef SHAPE
    /* overwrite miSetShape with our own */
    pScreen->SetShape = xnestSetShape;
#endif /* SHAPE */

    /* devPrivates */

#define POSITION_OFFSET (pScreen->myNum * (xnestWidth + xnestHeight) / 32)

    if (xnestDoFullGeneration) {

        valuemask = XCBCWBackPixel | XCBCWEventMask | XCBCWColormap;
        param.back_pixel = xnestWhitePixel;
        param.event_mask = xnestEventMask;
        param.colormap = xnestDefaultVisualColormap(xnestGetDefaultVisual(pScreen)).xid;

        if (xnestParentWindow.xid != 0) {
            xnestDefaultWindows[pScreen->myNum] = xnestParentWindow;
            XCBChangeWindowAttributes(xnestConnection,
                                      xnestDefaultWindows[pScreen->myNum],
                                      XCBCWEventMask,
                                      &xnestEventMask);
        } else {
            //vid.id = pScreen->rootVisual;
            vid = screen->root_visual;
            xnestDefaultWindows[pScreen->myNum] = XCBWINDOWNew(xnestConnection);
            XCBAuxCreateWindow(xnestConnection, 
                               xnestDefaultDepth,
                               xnestDefaultWindows[pScreen->myNum],
                               screen->root,
                               xnestX + POSITION_OFFSET,
                               xnestY + POSITION_OFFSET,
                               xnestWidth, xnestHeight,
                               xnestBorderWidth,
                               XCBWindowClassInputOutput,
                               vid,
                               valuemask, 
                               &param);
        }

        if (!xnestWindowName)
            xnestWindowName = argv[0];

        sizeHints = AllocSizeHints();
        SizeHintsSetFlagPPosition(sizeHints);
        SizeHintsSetFlagPSize(sizeHints);
        SizeHintsSetFlagPMaxSize(sizeHints);
        SizeHintsSetPosition(sizeHints, False, xnestX + POSITION_OFFSET, xnestY + POSITION_OFFSET);
        SizeHintsSetSize(sizeHints, 0, xnestWidth, xnestHeight);
        SizeHintsSetMaxSize(sizeHints, xnestWidth, xnestHeight);        
        if (xnestUserGeometry & XValue || xnestUserGeometry & YValue)
            SizeHintsSetFlagUSPosition(sizeHints);
        if (xnestUserGeometry & WidthValue || xnestUserGeometry & HeightValue)
            SizeHintsSetFlagUSSize(sizeHints);
        /*FIXME: set name and the like properly!*/
        SetWMNormalHints(xnestConnection, xnestDefaultWindows[pScreen->myNum], sizeHints);
        XCBMapWindow(xnestConnection, xnestDefaultWindows[pScreen->myNum]);

        valuemask = CWBackPixmap | CWColormap;
        param.back_pixmap = xnestScreenSaverPixmap.xid;
        param.colormap = screen->default_colormap.xid;
        xnestScreenSaverWindows[pScreen->myNum] = XCBWINDOWNew(xnestConnection);
        XCBAuxCreateWindow(xnestConnection,
                           screen->root_depth,
                           xnestScreenSaverWindows[pScreen->myNum],
                           xnestDefaultWindows[pScreen->myNum],
                           0, 0, /*x, y*/
                           xnestWidth, xnestHeight, 
                           0, /*border width*/
                           InputOutput,  /*class*/
                           screen->root_visual,
                           valuemask,
                           &param);
    }

    if (!xnestCreateDefaultColormap(pScreen)) return False;

    return True;
}

Bool xnestCloseScreen(int index, ScreenPtr pScreen)
{
    int i;

    for (i = 0; i < pScreen->numDepths; i++)
        xfree(pScreen->allowedDepths[i].vids);
    xfree(pScreen->allowedDepths);
    xfree(pScreen->visuals);
    xfree(pScreen->devPrivate);

    /*
       If xnestDoFullGeneration all x resources will be destroyed upon closing
       the display connection.  There is no need to generate extra protocol.
       */

    return True;
}
