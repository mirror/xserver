#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>

#include "xs-globals.h"
XCBConnection *xsConnection;
XCBDRAWABLE    xsDefaultDrawables[MAXDEPTH];
XCBDRAWABLE    xsDefaultWindow;
int            xsFontPrivateIndex;
int            xsGCPrivateIndex;
int            xsWindowPrivateIndex;
