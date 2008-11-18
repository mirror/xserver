/*
 * Copyright 2001-2004 Red Hat Inc., Durham, North Carolina.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL RED HAT AND/OR THEIR SUPPLIERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Authors:
 *   Rickard E. (Rik) Faith <faith@redhat.com>
 *
 */

/** \file
 * This code queries and modifies the connection block. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxcb.h"
#include "dmxinput.h"
#include "dmxlog.h"
#include "dmxwindow.h"

extern int     connBlockScreenStart;

#ifdef PANORAMIX
extern int     PanoramiXPixWidth;
extern int     PanoramiXPixHeight;
extern int     PanoramiXNumScreens;
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

       int     dmxGlobalWidth, dmxGlobalHeight;

/** We may want the wall dimensions to be different from the bounding
 * box dimensions that Xinerama computes, so save those and update them
 * here.
 */
void dmxSetWidthHeight(int width, int height)
{
    dmxGlobalWidth  = width;
    dmxGlobalHeight = height;
}

void dmxComputeWidthHeight(void)
{
    int       i;
    ScreenPtr pScreen;
    int       w = 0;
    int       h = 0;
    
    for (i = 0; i < dmxNumScreens; i++) {
	pScreen = screenInfo.screens[i];
        if (w < pScreen->width)
            w = pScreen->width;
        if (h < pScreen->height)
            h = pScreen->height;
    }

    dmxSetWidthHeight (w, h);
}

static Bool
dmxCreateInputOverlayWindow (void)
{
    WindowPtr pWin;
    XID       inputOverlayWid;
    XID       overrideRedirect = TRUE;
    int	      result;
    Atom      xdndVersion = 5;

    if (dmxScreens[0].inputOverlayWid)
	return TRUE;

    inputOverlayWid = FakeClientID (0);

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	PanoramiXRes *newWin;
	int          j;

	if (!(newWin = (PanoramiXRes *) xalloc (sizeof (PanoramiXRes))))
	    return BadAlloc;

	newWin->type = XRT_WINDOW;
	newWin->u.win.visibility = VisibilityNotViewable;
	newWin->u.win.class = InputOnly;
	newWin->u.win.root = FALSE;
	newWin->info[0].id = inputOverlayWid;
	for(j = 1; j < PanoramiXNumScreens; j++)
	    newWin->info[j].id = FakeClientID (0);

	FOR_NSCREENS_BACKWARD(j) {
	    pWin = CreateWindow (newWin->info[j].id, WindowTable[j],
				 -1, -1, 1, 1, 0, InputOnly, 
				 CWOverrideRedirect, &overrideRedirect,
				 0, serverClient, CopyFromParent, 
				 &result);
	    if (result != Success)
		return FALSE;
	    if (!AddResource (pWin->drawable.id, RT_WINDOW, pWin))
		return FALSE;

	    dmxScreens[j].inputOverlayWid = inputOverlayWid;
	    dmxScreens[j].pInputOverlayWin = pWin;
	}

	AddResource (newWin->info[0].id, XRT_WINDOW, newWin);
    }
    else
#endif
	
    {
	pWin = CreateWindow (inputOverlayWid, WindowTable[0],
			     -1, -1, 1, 1, 0, InputOnly, 
			     CWOverrideRedirect, &overrideRedirect,
			     0, serverClient, CopyFromParent, 
			     &result);
	if (result != Success)
	    return FALSE;
	if (!AddResource (pWin->drawable.id, RT_WINDOW, pWin))
	    return FALSE;

	dmxScreens[0].inputOverlayWid = inputOverlayWid;
	dmxScreens[0].pInputOverlayWin = pWin;
    }

    ChangeWindowProperty (dmxScreens[0].pInputOverlayWin,
			  dmxScreens[0].xdndAwareAtom,
			  XA_ATOM,
			  32,
			  PropModeReplace,
			  1,
			  &xdndVersion,
			  TRUE);

    return TRUE;
}

/** A callback routine that hooks into Xinerama and provides a
 * convenient place to print summary log information during server
 * startup.  This routine does not modify any values. */
void dmxConnectionBlockCallback(void)
{
    xWindowRoot *root   = (xWindowRoot *)(ConnectionInfo+connBlockScreenStart);
    int         offset  = connBlockScreenStart + sizeof(xWindowRoot);
    int         i;
    Bool        *found  = NULL;

    MAXSCREENSALLOC(found);
    if (!found)
        dmxLog(dmxFatal, "dmxConnectionBlockCallback: out of memory\n");

    dmxLog(dmxInfo, "===== Start of Summary =====\n");
#ifdef PANORAMIX
    if (!noPanoramiXExtension) {
        if (dmxGlobalWidth && dmxGlobalHeight
            && (dmxGlobalWidth != PanoramiXPixWidth
                || dmxGlobalHeight != PanoramiXPixHeight)) {
            dmxLog(dmxInfo,
                   "Changing Xinerama dimensions from %d %d to %d %d\n",
                   PanoramiXPixWidth, PanoramiXPixHeight,
                   dmxGlobalWidth, dmxGlobalHeight);
            PanoramiXPixWidth  = root->pixWidth  = dmxGlobalWidth;
            PanoramiXPixHeight = root->pixHeight = dmxGlobalHeight;
        } else {
            dmxGlobalWidth  = PanoramiXPixWidth;
            dmxGlobalHeight = PanoramiXPixHeight;
        }
        dmxLog(dmxInfo, "%d screens configured with Xinerama (%d %d)\n",
               PanoramiXNumScreens, PanoramiXPixWidth, PanoramiXPixHeight);
	for (i = 0; i < PanoramiXNumScreens; i++) found[i] = FALSE;
    } else {
#endif
                                /* This never happens because we're
                                 * either called from a Xinerama
                                 * callback or during reconfiguration
                                 * (which only works with Xinerama on).
                                 * In any case, be reasonable. */
        dmxLog(dmxInfo, "%d screens configured (%d %d)\n",
               screenInfo.numScreens, root->pixWidth, root->pixHeight);
#ifdef PANORAMIX
    }
#endif

    for (i = 0; i < root->nDepths; i++) {
        xDepth      *depth  = (xDepth *)(ConnectionInfo + offset);
        int         voffset = offset + sizeof(xDepth);
        xVisualType *visual = (xVisualType *)(ConnectionInfo + voffset);
        int         j;
        
        dmxLog(dmxInfo, "%d visuals at depth %d:\n",
               depth->nVisuals, depth->depth);
        for (j = 0; j < depth->nVisuals; j++, visual++) {
            XVisualInfo vi;
            
            vi.visual        = NULL;
            vi.visualid      = visual->visualID;
            vi.screen        = 0;
            vi.depth         = depth->depth;
            vi.class         = visual->class;
            vi.red_mask      = visual->redMask;
            vi.green_mask    = visual->greenMask;
            vi.blue_mask     = visual->blueMask;
            vi.colormap_size = visual->colormapEntries;
            vi.bits_per_rgb  = visual->bitsPerRGB;
            dmxLogVisual(NULL, &vi, 0);

#ifdef PANORAMIX
	    if (!noPanoramiXExtension) {
		int  k;
		for (k = 0; k < PanoramiXNumScreens; k++) {
		    DMXScreenInfo *dmxScreen = &dmxScreens[k];

		    if (dmxScreen->beDisplay) {
			XVisualInfo *pvi =
			    &dmxScreen->beVisuals[dmxScreen->beDefVisualIndex];
			if (pvi->depth == depth->depth &&
			    pvi->class == visual->class)
			    found[k] = TRUE;
		    } else {
			/* Screen #k is detatched, so it always succeeds */
			found[k] = TRUE;
		    }
		}
	    }
#endif
        }
        offset = voffset + depth->nVisuals * sizeof(xVisualType);
    }

    dmxLog(dmxInfo, "===== End of Summary =====\n");

#ifdef PANORAMIX
    if (!noPanoramiXExtension) {
	Bool fatal = FALSE;
	for (i = 0; i < PanoramiXNumScreens; i++) {
	    fatal |= !found[i];
	    if (!found[i]) {
		dmxLog(dmxError,
		       "The default visual for screen #%d does not match "
		       "any of the\n", i);
		dmxLog(dmxError,
		       "consolidated visuals from Xinerama (listed above)\n");
	    }
	}
	if (fatal)
            dmxLog(dmxFatal,
                   "dmxConnectionBlockCallback: invalid screen(s) found");
    }
#endif
    MAXSCREENSFREE(found);

    if (!dmxCreateInputOverlayWindow ())
	dmxLog (dmxFatal, "dmxCreateInputOverlayWindow: failed");
}
