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
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include "scrnintstr.h"
#include "dix.h"
#include "mi.h"
#include "mibstore.h"

static int           num_visuals = 0;
static xcb_visualtype_t *visuals;
static uint8_t         *depths;

static void xsInitVisuals(void)
{
    /*initialize the visuals*/
}

xcb_visualtype_t *xsGetVisual(VisualPtr pVisual)
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

static xcb_visualtype_t *visualFromID(ScreenPtr pScreen, xcb_visualid_t visual)
{
    int i;

    for (i = 0; i < pScreen->numVisuals; i++)
        if (pScreen->visuals[i].vid == visual)
            return xsGetVisual(&pScreen->visuals[i]);

    return NULL;
}

static xcb_visualtype_t *xsGetDefaultVisual(ScreenPtr pScreen)
{
    xcb_visualid_t v;

    v = pScreen->rootVisual;
    return visualFromID(pScreen, v);
}

/*
xcb_colormap_t xsDefaultVisualColormap(xcb_visualtype_t *visual)
{
    int i;
    xcb_colormap_t noneCmap = { 0 };

    for (i = 0; i < num_visuals; i++)
        if (&visuals[i] == visual)
            return xsDefaultColormaps[i];

    return noneCmap;
}
*/
