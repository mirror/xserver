#ifdef HAVE_XSCREEN_CONFIG_H
#include <xs-config.h>
#endif

#include <stdlib.h>

/* need to include Xmd before XCB stuff, or
 * things get redeclared.*/
#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/shape.h>
#include "scrnintstr.h"
#include "dix.h"
#include "mi.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "resource.h"

#include "mi.h"

#include "xs-globals.h"
#include "xs-gc.h"
#include "xs-font.h"
#include "xs-gcops.h"
#include "xs-screen.h"
#include "xs-window.h"
#include "xs-pixmap.h"
#include "xs-color.h"


/*sets up screensaver*/
static Bool xsSaveScreen(ScreenPtr pScreen, int action)
{
    /* It makes absolutely no sense to support a screensaver
     * in a rootless nested X server, so I'm just returning FALSE
     * here. Hopefully this is correct.*/
    return FALSE;
}

/**
 * Initialize the function pointers in pScreen.
 * Just grouped together here for readability.
 **/
static void xsScreenSetProcs(ScreenPtr pScreen)
{
    /* Random screen procedures */
    pScreen->QueryBestSize = xsQueryBestSize;
    pScreen->SaveScreen = xsSaveScreen;
    pScreen->GetImage = xsGetImage;
    pScreen->GetSpans = xsGetSpans;
    pScreen->PointerNonInterestBox = NULL;
    pScreen->SourceValidate = NULL;

    /* Window Procedures */
    pScreen->CreateWindow = xsCreateWindow;
    pScreen->DestroyWindow = xsDestroyWindow;
    pScreen->PositionWindow = xsPositionWindow;
    pScreen->ChangeWindowAttributes = xsChangeWindowAttributes;
    pScreen->RealizeWindow = xsRealizeWindow;
    pScreen->UnrealizeWindow = xsUnrealizeWindow;
    pScreen->PostValidateTree = NULL;
    //pScreen->WindowExposures = xsWindowExposures;
    pScreen->PaintWindowBackground = xsPaintWindowBackground;
    pScreen->PaintWindowBorder = xsPaintWindowBorder;
    pScreen->CopyWindow = xsCopyWindow;
    pScreen->ClipNotify = xsClipNotify;

    /* Backing store procedures */
    pScreen->SaveDoomedAreas = NULL;
    pScreen->RestoreAreas = NULL;
    pScreen->ExposeCopy = NULL;
    pScreen->TranslateBackingStore = NULL;
    pScreen->ClearBackingStore = NULL;
    pScreen->DrawGuarantee = NULL;

    /* Font procedures */
    pScreen->RealizeFont = xsRealizeFont;
    pScreen->UnrealizeFont = xsUnrealizeFont;

    /* GC procedures */
    pScreen->CreateGC = xsCreateGC;

    /* Colormap procedures */
    pScreen->CreateColormap = xsCreateColormap;
    pScreen->DestroyColormap = xsDestroyColormap;
    pScreen->InstallColormap = xsInstallColormap;
    pScreen->UninstallColormap = xsUninstallColormap;
    pScreen->ListInstalledColormaps = (ListInstalledColormapsProcPtr) xsListInstalledColormaps;
    pScreen->StoreColors = (StoreColorsProcPtr) xsStoreColors;
    pScreen->ResolveColor = xsResolveColor;

    pScreen->BitmapToRegion = xsPixmapToRegion;

    /* OS layer procedures */
    pScreen->BlockHandler = (ScreenBlockHandlerProcPtr)NoopDDA;
    pScreen->WakeupHandler = (ScreenWakeupHandlerProcPtr)NoopDDA;
    pScreen->blockData = NULL;
    pScreen->wakeupData = NULL;

    #ifdef SHAPE
    /* overwrite miSetShape with our own */
    pScreen->SetShape = xsSetShape;
    #endif /* SHAPE */
}

Bool xsOpenScreen(int index, ScreenPtr pScreen, int argc, char *argv[])
{
    xsScreenSetProcs(pScreen);
    return TRUE;
}
