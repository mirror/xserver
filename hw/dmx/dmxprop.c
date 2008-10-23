/*
 * Copyright 2002-2003 Red Hat Inc., Durham, North Carolina.
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
 *
 * It is possible for one of the DMX "backend displays" to actually be
 * smaller than the dimensions of the backend X server.  Therefore, it
 * is possible for more than one of the DMX "backend displays" to be
 * physically located on the same backend X server.  This situation must
 * be detected so that cursor motion can be handled in an expected
 * fashion.
 *
 * We could analyze the names used for the DMX "backend displays" (e.g.,
 * the names passed to the -display command-line parameter), but there
 * are many possible names for a single X display, and failing to detect
 * sameness leads to very unexpected results.  Therefore, whenever the
 * DMX server opens a window on a backend X server, a property value is
 * queried and set on that backend to detect when another window is
 * already open on that server.
 *
 * Further, it is possible that two different DMX server instantiations
 * both have windows on the same physical backend X server.  This case
 * is also detected so that pointer input is not taken from that
 * particular backend X server.
 *
 * The routines in this file handle the property management. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxprop.h"
#include "dmxwindow.h"
#include "dmxlog.h"
#include "dmxatom.h"
#include "dmxselection.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

static int (*dmxSaveProcVector[256]) (ClientPtr);

static int
dmxProcChangeProperty (ClientPtr client)
{
    WindowPtr   pWin;
    PropertyPtr pProp;
    int         err;
    REQUEST(xChangePropertyReq);

    err = (*dmxSaveProcVector[X_ChangeProperty]) (client);
    if (err != Success)
	return err;

    if (dixLookupWindow (&pWin,
			 stuff->window,
			 serverClient,
			 DixReadAccess) != Success ||
	dixLookupProperty (&pProp,
			   pWin,
			   stuff->property,
			   serverClient,
			   DixReadAccess) != Success)
	return Success;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	PanoramiXRes *win;
	int          j;

	if ((win = (PanoramiXRes *) SecurityLookupIDByType (serverClient,
							    stuff->window,
							    XRT_WINDOW,
							    DixReadAccess)))
	{
	    FOR_NSCREENS_BACKWARD(j) {
		WindowPtr pScrWin;

		if (dixLookupWindow (&pScrWin,
				     win->info[j].id,
				     serverClient,
				     DixReadAccess) == Success)
		    dmxBESetWindowProperty (pScrWin, pProp);
	    }
	}
	
	dmxSelectionPropertyChangeCheck (pWin,
					 stuff->property,
					 stuff->nUnits);

	return Success;
    }
#endif

    dmxBESetWindowProperty (pWin, pProp);

    dmxSelectionPropertyChangeCheck (pWin,
				     stuff->property,
				     stuff->nUnits);

    return Success;
}

static void
dmxDeleteProperty (WindowPtr pWin,
		   Atom      property)
{
    ScreenPtr     pScreen = pWin->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Window        window;

    window = dmxBEGetSelectionAdjustedPropertyWindow (pWin);
    if (!window)
	return;

    XLIB_PROLOGUE (dmxScreen);
    XDeleteProperty (dmxScreen->beDisplay,
		     window,
		     dmxBEAtom (dmxScreen, property));
    XLIB_EPILOGUE (dmxScreen);
}

static int
dmxProcDeleteProperty (ClientPtr client)
{
    WindowPtr pWin;
    int       err;
    REQUEST(xDeletePropertyReq);

    err = (*dmxSaveProcVector[X_DeleteProperty]) (client);
    if (err != Success)
	return err;

    if (dixLookupWindow (&pWin,
			 stuff->window,
			 serverClient,
			 DixReadAccess) != Success)
	return err;


#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	PanoramiXRes *win;
	int          j;

	if ((win = (PanoramiXRes *) SecurityLookupIDByType (serverClient,
							    stuff->window,
							    XRT_WINDOW,
							    DixReadAccess)))
	{
	    FOR_NSCREENS_BACKWARD(j) {
		WindowPtr pScrWin;

		if (dixLookupWindow (&pScrWin,
				     win->info[j].id,
				     serverClient,
				     DixReadAccess) == Success)
		    dmxDeleteProperty (pScrWin, stuff->property);
	    }
	}

	dmxSelectionPropertyChangeCheck (pWin,
					 stuff->property,
					 -1);

	return Success;
    }
#endif

    dmxDeleteProperty (pWin, stuff->property);

    dmxSelectionPropertyChangeCheck (pWin,
				     stuff->property,
				     -1);

    return Success;
}

static int
dmxProcGetProperty (ClientPtr client)
{
    WindowPtr   pWin;
    PropertyPtr pProp;
    int         err;
    REQUEST(xGetPropertyReq);

    err = (*dmxSaveProcVector[X_GetProperty]) (client);
    if (err != Success || !stuff->delete)
	return err;

    if (dixLookupWindow (&pWin,
			 stuff->window,
			 serverClient,
			 DixReadAccess) != Success ||
	dixLookupProperty (&pProp,
			   pWin,
			   stuff->property,
			   serverClient,
			   DixReadAccess) != BadMatch)
	return Success;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	PanoramiXRes *win;
	int          j;

	if ((win = (PanoramiXRes *) SecurityLookupIDByType (serverClient,
							    stuff->window,
							    XRT_WINDOW,
							    DixReadAccess)))
	{
	    FOR_NSCREENS_BACKWARD(j) {
		WindowPtr pScrWin;

		if (dixLookupWindow (&pScrWin,
				     win->info[j].id,
				     serverClient,
				     DixReadAccess) == Success)
		{
		    dmxDeleteProperty (pScrWin, stuff->property);
		}
	    }
	}

	dmxSelectionPropertyChangeCheck (pWin,
					 stuff->property,
					 -1);

	return Success;
    }
#endif

    dmxDeleteProperty (pWin, stuff->property);

    dmxSelectionPropertyChangeCheck (pWin,
				     stuff->property,
				     -1);

    return Success;
}

static void
dmxRotateProperties (WindowPtr pWin,
		     Atom      *atoms,
		     Atom      *buf,
		     int       nAtoms,
		     int       nPositions)
{
    ScreenPtr     pScreen = pWin->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Window        window;
    int		  i;

    window = dmxBEGetSelectionAdjustedPropertyWindow (pWin);
    if (!window)
	return;

    for (i = 0; i < nAtoms; i++)
	buf[i] = dmxBEAtom (dmxScreen, atoms[i]);

    XLIB_PROLOGUE (dmxScreen);
    XRotateWindowProperties (dmxScreen->beDisplay,
			     window,
			     buf,
			     nAtoms,
			     nPositions);
    XLIB_EPILOGUE (dmxScreen);
}

static int
dmxProcRotateProperties (ClientPtr client)
{
    WindowPtr pWin;
    int       err;
    Atom      *buf, *atoms;
    REQUEST(xRotatePropertiesReq);

    err = (*dmxSaveProcVector[X_RotateProperties]) (client);
    if (err != Success)
	return err;

    atoms = (Atom *) & stuff[1];
    buf   = (Atom *) xalloc (stuff->nAtoms * sizeof (Atom));
    if (!buf)
	return Success;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	PanoramiXRes *win;
	int          j;

	if ((win = (PanoramiXRes *) SecurityLookupIDByType (serverClient,
							    stuff->window,
							    XRT_WINDOW,
							    DixReadAccess)))
	{
	    FOR_NSCREENS_FORWARD(j) {
		if (dixLookupWindow (&pWin,
				     win->info[j].id,
				     serverClient,
				     DixReadAccess) == Success)
		    dmxRotateProperties (pWin, atoms, buf, stuff->nAtoms,
					 stuff->nPositions);
	    }
	}

	xfree (buf);

	return Success;
    }
#endif

    if (dixLookupWindow (&pWin,
			 stuff->window,
			 serverClient,
			 DixReadAccess) == Success)
	dmxRotateProperties (pWin, atoms, buf, stuff->nAtoms,
			     stuff->nPositions);

    xfree (buf);

    return Success;
}

/** Initialize property support.  In addition to the screen function call
 *  pointers, DMX also hooks in at the ProcVector[] level.  Here the old
 *  ProcVector function pointers are saved and the new ProcVector
 *  function pointers are initialized. */
void dmxInitProps (void)
{
    int  i;

    for (i = 0; i < 256; i++)
	dmxSaveProcVector[i] = ProcVector[i];

    ProcVector[X_ChangeProperty]   = dmxProcChangeProperty;
    ProcVector[X_DeleteProperty]   = dmxProcDeleteProperty;
    ProcVector[X_GetProperty]      = dmxProcGetProperty;
    ProcVector[X_RotateProperties] = dmxProcRotateProperties;
}

/** Reset property support by restoring the original ProcVector function
 *  pointers. */
void dmxResetProps (void)
{
    int  i;

    for (i = 0; i < 256; i++)
	ProcVector[i] = dmxSaveProcVector[i];
}
