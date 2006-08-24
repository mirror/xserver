/* $Xorg: Visual.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
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
/* $XFree86$ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xs-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xproto.h>
#include "scrnintstr.h"
#include "dix.h"
#include "mi.h"
#include "mibstore.h"

static int           num_visuals = 0;
static XCBVISUALTYPE *visuals;
static CARD8         *depths;

void xsInitVisuals()
{
    /*initialize the visuals*/
}

XCBVISUALTYPE *xsGetVisual(VisualPtr pVisual)
{
    int i;

    for (i = 0; i < num_visuals; i++)
        if (pVisual->class == visuals[i]._class &&
                pVisual->bitsPerRGBValue == visuals[i].bits_per_rgb_value &&
                pVisual->ColormapEntries == visuals[i].colormap_entries &&
                pVisual->nplanes == depths[i] &&
                pVisual->redMask == visuals[i].red_mask &&
                pVisual->greenMask == visuals[i].green_mask &&
                pVisual->blueMask == visuals[i].blue_mask)
            return &visuals[i];

    return NULL;
}

XCBVISUALTYPE *visualFromID(ScreenPtr pScreen, XCBVISUALID visual)
{
    int i;

    for (i = 0; i < pScreen->numVisuals; i++)
        if (pScreen->visuals[i].vid == visual.id)
            return xsGetVisual(&pScreen->visuals[i]);

    return NULL;
}

XCBVISUALTYPE *xsGetDefaultVisual(ScreenPtr pScreen)
{
    XCBVISUALID v;

    v.id = pScreen->rootVisual;
    return visualFromID(pScreen, v);
}

/*
XCBCOLORMAP xsDefaultVisualColormap(XCBVISUALTYPE *visual)
{
    int i;
    XCBCOLORMAP noneCmap = { 0 };

    for (i = 0; i < num_visuals; i++)
        if (&visuals[i] == visual)
            return xsDefaultColormaps[i];

    return noneCmap;
}
*/
