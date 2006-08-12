#ifdef HAVE_XSCREEN_CONFIG_H
#include <xnest-config.h>
#endif

#include <stdlib.h>

/* need to include Xmd before XCB stuff, or
 * things get redeclared.*/
#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/shape.h>

#include "gcstruct.h"
#include "window.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "colormapst.h"
#include "scrnintstr.h"
#include "region.h"

#include "xs-globals.h"
#include "xs-screen.h"

#include "mi.h"

Bool xsOpenScreen(int index, ScreenPtr pScreen, int argc, char *argv[])
{
    return FALSE;
}
