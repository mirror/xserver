/**
 * Copyright 2006 Ori Bernstein
 *
 *    Permission to use, copy, modify, distribute, and sell this software
 *    and its documentation for any purpose is hereby granted without fee,
 *    provided that the above copyright notice appear in all copies and that
 *    both that copyright notice and this permission notice appear in
 *    supporting documentation.  Ori Bernstein makes no representations about
 *    the suitability of this software for any purpose.  It is provided "as
 *    is" without express or implied warranty.
 **/

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

#include "mi.h"

/**
 * DIX hook for processing input events.
 * Just hooks into the mi stuff.
 **/
void ProcessInputEvents()
{
    mieqProcessInputEvents();
    miPointerUpdate();
}

/*The backing server should have already filtered invalid modifiers*/
Bool LegalModifier(unsigned int key, DevicePtr pDev)
{
    return TRUE;
}

void OsVendorInit()
{
}

