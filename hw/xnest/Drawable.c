/* $Xorg: Window.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

   Copyright 2006 by Ori Bernstein

   Permission to use, copy, modify, distribute, and sell this software
   and its documentation for any purpose is hereby granted without fee,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation.  Ori Bernstein makes no representations about
   the suitability of this software for any purpose.  It is provided "as
   is" without express or implied warranty.

*/
/* $XFree86: xc/programs/Xserver/hw/xnest/Window.c,v 3.7 2001/10/28 03:34:11 tsi Exp $ */
#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>

#include "gcstruct.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "region.h"
#include "servermd.h"

#include "XNWindow.h"
#include "XNPixmap.h"
#include "Display.h"

XCBDRAWABLE xnestDrawable(DrawablePtr pDrawable)
{
    XCBDRAWABLE d;
    if (pDrawable->type == DRAWABLE_WINDOW)
        d.window = xnestWindow((WindowPtr)pDrawable);
    else 
        d.pixmap = xnestPixmapPriv((PixmapPtr)pDrawable)->pixmap;
    return d;
}        
