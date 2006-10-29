#include <X11/Xmd.h>
#include <xcb/xcb.h>
#include "screenint.h"
#include "scrnintstr.h"

#include "xs-globals.h"
#include "xs-gc.h"
#include "xs-window.h"

xcb_connection_t *xsConnection;
xcb_drawable_t    xsDefaultDrawables[MAXDEPTH];
xcb_drawable_t    xsBackingRoot;
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

