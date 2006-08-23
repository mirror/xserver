#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include "screenint.h"
#include "scrnintstr.h"

#include "xs-globals.h"
#include "xs-gc.h"
#include "xs-window.h"

XCBConnection *xsConnection;
XCBDRAWABLE    xsDefaultDrawables[MAXDEPTH];
XCBDRAWABLE    xsBackingRoot;
int            xsFontPrivateIndex;
int            xsGCPrivateIndex;
int            xsWindowPrivateIndex;

void xsAllocPrivateIndecies(ScreenPtr pScreen)
{
    xsFontPrivateIndex   = AllocateFontPrivateIndex();
    xsGCPrivateIndex     = AllocateGCPrivateIndex();
    xsWindowPrivateIndex = AllocateWindowPrivateIndex();
    AllocateWindowPrivate(pScreen, xsWindowPrivateIndex, sizeof(XscreenPrivWindow));
    AllocateGCPrivate(pScreen, xsGCPrivateIndex, sizeof(XscreenPrivGC));
}

