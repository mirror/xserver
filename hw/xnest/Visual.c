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
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xproto.h>
#include "scrnintstr.h"
#include "dix.h"
#include "mi.h"
#include "mibstore.h"
#include "Xnest.h"

#include "Display.h"
#include "Visual.h"

XCBVISUALTYPE *xnestVisual(VisualPtr pVisual)
{
    int i;

    for (i = 0; i < xnestNumVisuals; i++)
        if (pVisual->class == xnestVisuals[i]->_class &&
                pVisual->bitsPerRGBValue == xnestVisuals[i]->bits_per_rgb_value &&
                pVisual->ColormapEntries == xnestVisuals[i]->colormap_entries &&
                /*pVisual->nplanes == xnestVisuals[i]->depth && er. help*/
                pVisual->redMask == xnestVisuals[i]->red_mask &&
                pVisual->greenMask == xnestVisuals[i]->green_mask &&
                pVisual->blueMask == xnestVisuals[i]->blue_mask)
            return xnestVisuals[i];

    return NULL;
}

XCBVISUALTYPE *xnestVisualFromID(ScreenPtr pScreen, XCBVISUALID visual)
{
    int i;

    for (i = 0; i < pScreen->numVisuals; i++)
        if (pScreen->visuals[i].vid == visual.id)
            return xnestVisual(&pScreen->visuals[i]);

    return NULL;
}

XCBVISUALTYPE *xnestGetDefaultVisual(ScreenPtr pScreen)
{
    XCBVISUALID v;

    v.id = pScreen->rootVisual;
    return xnestVisualFromID(pScreen, v);
}

XCBCOLORMAP xnestDefaultVisualColormap(XCBVISUALTYPE *visual)
{
    int i;
    XCBCOLORMAP noneCmap = { 0 };

    for (i = 0; i < xnestNumVisuals; i++)
        if (xnestVisuals[i] == visual)
            return xnestDefaultColormaps[i];

    return noneCmap;
}
