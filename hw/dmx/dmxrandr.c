/*
 * Copyright © 2008 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#ifdef RANDR
#include "dmx.h"
#include "dmxlog.h"
#include "dmxextension.h"
#include "dmxcb.h"
#include "dmxrandr.h"
#include "dmxclient.h"
#include "dmxatom.h"
#include "dmxwindow.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

#include <xcb/randr.h>

static int xRROutputsForFirstScreen = 1;
static int xRRCrtcsForFirstScreen = 1;

static DMXScreenInfo *
dmxRRGetScreenForCrtc (ScreenPtr pScreen,
		       RRCrtcPtr crtc)
{
    int i;

    rrScrPriv (pScreen);

    for (i = 0; i < pScrPriv->numCrtcs; i++)
	if (pScrPriv->crtcs[i] == crtc)
	    break;

    if (i == pScrPriv->numCrtcs)
	return NULL;

    if (i < xRRCrtcsForFirstScreen)
	return dmxScreens;

    return &dmxScreens[((i - xRRCrtcsForFirstScreen) / xRRCrtcsPerScreen) + 1];
}

static DMXScreenInfo *
dmxRRGetScreenForOutput (ScreenPtr   pScreen,
			 RROutputPtr output)
{
    int i;

    rrScrPriv (pScreen);

    for (i = 0; i < pScrPriv->numOutputs; i++)
	if (pScrPriv->outputs[i] == output)
	    break;

    if (i == pScrPriv->numOutputs)
	return NULL;

    if (i < xRROutputsForFirstScreen)
	return dmxScreens;

    return &dmxScreens[((i - xRROutputsForFirstScreen) / xRROutputsPerScreen) +
		       1];
}

static RRModePtr
dmxRRGetMode (XRRScreenResources *r,
	      unsigned long	 mode)
{
    xRRModeInfo modeInfo;
    int		i;

    for (i = 0; i < r->nmode; i++)
    {
	if (r->modes[i].id == mode)
	{
	    memset (&modeInfo, '\0', sizeof (modeInfo));

	    modeInfo.width      = r->modes[i].width;
	    modeInfo.height     = r->modes[i].height;
	    modeInfo.dotClock   = r->modes[i].dotClock;
	    modeInfo.hSyncStart = r->modes[i].hSyncStart;
	    modeInfo.hSyncEnd   = r->modes[i].hSyncEnd;
	    modeInfo.hTotal     = r->modes[i].hTotal;
	    modeInfo.hSkew      = r->modes[i].hSkew;
	    modeInfo.vSyncStart = r->modes[i].vSyncStart;
	    modeInfo.vSyncEnd   = r->modes[i].vSyncEnd;
	    modeInfo.vTotal     = r->modes[i].vTotal;
	    modeInfo.nameLength = strlen (r->modes[i].name);
	    modeInfo.modeFlags  = r->modes[i].modeFlags;

	    return RRModeGet (&modeInfo, r->modes[i].name);
	}
    }

    return NULL;
}

static RRCrtcPtr
dmxRRGetCrtc (ScreenPtr     pScreen,
	      DMXScreenInfo *dmxScreen,
	      unsigned long crtc)
{
    int baseCrtc = 0;
    int numCrtc = xRRCrtcsForFirstScreen;
    int i;

    rrScrPriv (pScreen);

    if (!crtc)
	return NULL;

    if (dmxScreen != dmxScreens)
    {
	baseCrtc = xRRCrtcsForFirstScreen +
	    ((dmxScreen - dmxScreens) - 1) * xRRCrtcsPerScreen;
	numCrtc  = xRRCrtcsPerScreen;
    }

    for (i = 0; i < numCrtc; i++)
	if (pScrPriv->crtcs[baseCrtc + i]->devPrivate == (void *) crtc)
	    return pScrPriv->crtcs[baseCrtc + i];

    return NULL;
}

static RROutputPtr
dmxRRGetOutput (ScreenPtr     pScreen,
		DMXScreenInfo *dmxScreen,
		unsigned long output)
{
    int baseOutput = 0;
    int numOutput = xRROutputsForFirstScreen;
    int i;

    rrScrPriv (pScreen);

    if (!output)
	return NULL;

    if (dmxScreen != dmxScreens)
    {
	baseOutput = xRROutputsForFirstScreen +
	    ((dmxScreen - dmxScreens) - 1) * xRROutputsPerScreen;
	numOutput  = xRROutputsPerScreen;
    }

    for (i = 0; i < numOutput; i++)
	if (pScrPriv->outputs[baseOutput + i]->devPrivate == (void *) output)
	    return pScrPriv->outputs[baseOutput + i];

    return NULL;
}

static Bool
dmxRRUpdateCrtc (ScreenPtr	    pScreen,
		 DMXScreenInfo      *dmxScreen,
		 XRRScreenResources *r,
		 unsigned long	    xcrtc)
{
    XRRCrtcInfo   *c = NULL;
    RRCrtcPtr	  crtc;
    RRModePtr	  mode = NULL;
    RROutputPtr	  *outputs = NULL;
    XRRCrtcGamma  *gamma = NULL;
    int		  i, noutput = 0;

    crtc = dmxRRGetCrtc (pScreen, dmxScreen, xcrtc);
    if (!crtc)
	return TRUE; /* do nothing if the crtc doesn't exist */

    XLIB_PROLOGUE (dmxScreen);
    c = XRRGetCrtcInfo (dmxScreen->beDisplay, r, xcrtc);
    XLIB_EPILOGUE (dmxScreen);

    if (!c)
	return FALSE;

    if (c->noutput)
    {
	outputs = xalloc (sizeof (RROutputPtr) * c->noutput);
	if (!outputs)
	    return FALSE;
    }

    if (c->mode)
	mode = dmxRRGetMode (r, c->mode);

    for (i = 0; i < c->noutput; i++)
    {
	outputs[noutput] = dmxRRGetOutput (pScreen, dmxScreen, c->outputs[i]);
	if (outputs[noutput])
	    noutput++;
    }

    XLIB_PROLOGUE (dmxScreen);
    gamma = XRRGetCrtcGamma (dmxScreen->beDisplay, xcrtc);
    XLIB_EPILOGUE (dmxScreen);

    if (!gamma)
    {
	if (mode)
	    RRModeDestroy (mode);
	
	return FALSE;
    }

    RRCrtcGammaSet (crtc, gamma->red, gamma->green, gamma->blue);

    XRRFreeGamma (gamma);

    RRCrtcNotify (crtc, mode, c->x, c->y, c->rotation, noutput, outputs);

    if (outputs)
	xfree (outputs);

    if (mode)
	RRModeDestroy (mode);

    XRRFreeCrtcInfo (c);

    return TRUE;
}

static Bool
dmxRRUpdateOutput (ScreenPtr	      pScreen,
		   DMXScreenInfo      *dmxScreen,
		   XRRScreenResources *r,
		   unsigned long      xoutput)
{
    XRROutputInfo *o = NULL;
    RROutputPtr	  output, *clones = NULL;
    RRModePtr	  *modes = NULL;
    RRCrtcPtr	  *crtcs = NULL;
    int		  i, nclone = 0, ncrtc = 0;

    output = dmxRRGetOutput (pScreen, dmxScreen, xoutput);
    if (!output)
	return TRUE; /* do nothing if the output doesn't exist */

    XLIB_PROLOGUE (dmxScreen);
    o = XRRGetOutputInfo (dmxScreen->beDisplay, r, xoutput);
    XLIB_EPILOGUE (dmxScreen);

    if (!o)
	return FALSE;

    if (o->nclone)
    {
	clones = xalloc (sizeof (RROutputPtr) * o->nclone);
	if (!clones)
	    return FALSE;
    }

    if (o->nmode)
    {
	modes = xalloc (sizeof (RRModePtr) * o->nmode);
	if (!modes)
	    return FALSE;
    }

    if (o->ncrtc)
    {
	crtcs = xalloc (sizeof (RRCrtcPtr) * o->ncrtc);
	if (!crtcs)
	    return FALSE;
    }

    for (i = 0; i < o->nclone; i++)
    {
	clones[nclone] = dmxRRGetOutput (pScreen, dmxScreen, o->clones[i]);
	if (clones[nclone])
	    nclone++;
    }

    for (i = 0; i < o->ncrtc; i++)
    {
	crtcs[ncrtc] = dmxRRGetCrtc (pScreen, dmxScreen, o->crtcs[i]);
	if (crtcs[ncrtc])
	    ncrtc++;
    }

    for (i = 0; i < o->nmode; i++)
    {
	modes[i] = dmxRRGetMode (r, o->modes[i]);
	if (!modes[i])
	    return FALSE;
    }

    if (!RROutputSetClones (output, clones, nclone))
	return FALSE;

    if (!RROutputSetModes (output, modes, o->nmode, o->npreferred))
	return FALSE;

    if (!RROutputSetCrtcs (output, crtcs, ncrtc))
	return FALSE;

    if (!RROutputSetConnection (output, o->connection))
	return FALSE;

    if (!RROutputSetSubpixelOrder (output, o->subpixel_order))
	return FALSE;

    if (!RROutputSetPhysicalSize (output, o->mm_width, o->mm_height))
	return FALSE;

    if (clones)
	xfree (clones);

    if (modes)
	xfree (modes);

    if (crtcs)
	xfree (crtcs);

    XRRFreeOutputInfo (o);

    return TRUE;
}

static Bool
dmxRRUpdateOutputProperty (ScreenPtr	      pScreen,
			   DMXScreenInfo      *dmxScreen,
			   XRRScreenResources *r,
			   unsigned long      xoutput,
			   unsigned long      xproperty)
{
    RROutputPtr     output;
    XRRPropertyInfo *info = NULL;
    unsigned char   *prop;
    int		    format, status = !Success;
    unsigned long   nElements, bytesAfter;
    Atom	    type, atom;
    INT32           *values = NULL;

    output = dmxRRGetOutput (pScreen, dmxScreen, xoutput);
    if (!output)
	return TRUE; /* do nothing if the output doesn't exist */

    atom = dmxAtom (dmxScreen, xproperty);

    XLIB_PROLOGUE (dmxScreen);
    status = XRRGetOutputProperty (dmxScreen->beDisplay, xoutput, xproperty,
				   0, 8192, FALSE, FALSE,
				   AnyPropertyType, &type, &format,
				   &nElements, &bytesAfter, &prop);
    XLIB_EPILOGUE (dmxScreen);

    if (status != Success)
	return FALSE;

    XLIB_PROLOGUE (dmxScreen);
    info = XRRQueryOutputProperty (dmxScreen->beDisplay, xoutput, xproperty);
    XLIB_EPILOGUE (dmxScreen);

    if (!info)
	return FALSE;

    if (info->num_values)
    {
	int i;

	values = xalloc (info->num_values * sizeof (INT32));
	if (!values)
	    return FALSE;

	for (i = 0; i < info->num_values; i++)
	    values[i] = info->values[i];
    }

    if (type == XA_ATOM && format == 32)
    {
	INT32 *atoms = (INT32 *) prop;
	int   i;

	for (i = 0; i < nElements; i++)
	    atoms[i] = dmxAtom (dmxScreen, atoms[i]);

	if (!info->range && info->num_values > 0)
	{
	    for (i = 0; i < info->num_values; i++)
		values[i] = dmxAtom (dmxScreen, values[i]);
	}
    }

    RRConfigureOutputProperty (output, atom, FALSE,
			       info->range, info->immutable, info->num_values,
			       values);

    RRChangeOutputProperty (output, atom, type, format, PropModeReplace,
			    nElements, prop, FALSE, TRUE);

    if (values)
	xfree (values);

    XFree (info);
    XFree (prop);

    return TRUE;
}

static Bool
dmxRRGetInfo (ScreenPtr pScreen,
	      Rotation  *rotations)
{
    int	i;

    rrScrPriv (pScreen);

    if (pScreen->myNum)
    {
	*rotations = RR_Rotate_0;
	return TRUE;
    }

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo      *dmxScreen = &dmxScreens[i];
	XRRScreenResources *r = NULL;
	int		   outputsPerScreen = xRROutputsForFirstScreen;
	int		   baseOutput = 0;
	int		   crtcsPerScreen = xRRCrtcsForFirstScreen;
	int		   baseCrtc = 0;
	int		   j;

	if (i)
	{
	    outputsPerScreen = xRROutputsPerScreen;
	    baseOutput       = xRROutputsForFirstScreen +
		(i - 1) * xRROutputsPerScreen;
	    crtcsPerScreen   = xRRCrtcsPerScreen;
	    baseCrtc         = xRRCrtcsForFirstScreen +
		(i - 1) * xRRCrtcsPerScreen;
	}

	assert (baseOutput + outputsPerScreen <= pScrPriv->numOutputs);
	assert (baseCrtc + crtcsPerScreen <= pScrPriv->numCrtcs);

	dmxScreen->beRandrPending = TRUE;

	if (dmxScreen->beRandr && dmxScreen->beDisplay)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    r = XRRGetScreenResources (dmxScreen->beDisplay,
				       dmxScreen->scrnWin);
	    XLIB_EPILOGUE (dmxScreen);

	    if (r)
	    {
		if (r->noutput > outputsPerScreen)
		    dmxLog (dmxWarning,
			    "dmxRRGetInfo: ignoring %d BE server outputs\n",
			    r->noutput - outputsPerScreen);

		if (r->ncrtc > crtcsPerScreen)
		    dmxLog (dmxWarning,
			    "dmxRRGetInfo: ignoring %d BE server crtcs\n",
			    r->ncrtc - crtcsPerScreen);
	    }
	}

	for (j = 0; j < outputsPerScreen; j++)
	{
	    RROutputPtr output = pScrPriv->outputs[baseOutput + j];

	    if (r && j < r->noutput)
		output->devPrivate = (void *) r->outputs[j];
	    else
		output->devPrivate = NULL;
	}

	for (j = 0; j < crtcsPerScreen; j++)
	{
	    RRCrtcPtr crtc = pScrPriv->crtcs[baseCrtc + j];

	    crtc->devPrivate = NULL;

	    if (r && j < r->ncrtc)
		crtc->devPrivate = (void *) r->crtcs[j];
	    else
		crtc->devPrivate = NULL;
	}

	for (j = 0; j < outputsPerScreen; j++)
	{
	    RROutputPtr output = pScrPriv->outputs[baseOutput + j];

	    if (r)
	    {
		if (j < r->noutput)
		{
#ifdef _XSERVER64
		    Atom64 *props = NULL;
#else
		    Atom   *props = NULL;
#endif

		    int    nProp = 0, k;

		    if (!dmxRRUpdateOutput (pScreen,
					    dmxScreen,
					    r,
					    r->outputs[j]))
			return (dmxScreen->beRandrPending = FALSE);

		    XLIB_PROLOGUE (dmxScreen);
		    props = XRRListOutputProperties (dmxScreen->beDisplay,
						     r->outputs[j],
						     &nProp);
		    XLIB_EPILOGUE (dmxScreen);

		    if (nProp)
		    {
			for (k = 0; k < nProp; k++)
			    if (!dmxRRUpdateOutputProperty (pScreen,
							    dmxScreen,
							    r,
							    r->outputs[j],
							    props[k]))
				return (dmxScreen->beRandrPending = FALSE);

			XFree (props);
		    }
		}
		else
		{
		    if (!RROutputSetModes (output, NULL, 0, 0))
			return (dmxScreen->beRandrPending = FALSE);
		    if (!RROutputSetClones (output, NULL, 0))
			return (dmxScreen->beRandrPending = FALSE);
		    if (!RROutputSetCrtcs (output, NULL, 0))
			return (dmxScreen->beRandrPending = FALSE);
		    if (!RROutputSetConnection (output, RR_Disconnected))
			return (dmxScreen->beRandrPending = FALSE);
		}
	    }
	    else if (dmxScreen->beDisplay && j == 0)
	    {
		RRModePtr   mode;
		xRRModeInfo modeInfo;
		char	    name[64];

		sprintf (name,
			 "%dx%d",
			 dmxScreen->scrnWidth, dmxScreen->scrnHeight);

		memset (&modeInfo, '\0', sizeof (modeInfo));
		modeInfo.width = dmxScreen->scrnWidth;
		modeInfo.height = dmxScreen->scrnHeight;
		modeInfo.nameLength = strlen (name);

		mode = RRModeGet (&modeInfo, name);
		if (!mode)
		    return (dmxScreen->beRandrPending = FALSE);

		if (!RROutputSetModes (output, &mode, 1, 0))
		    return (dmxScreen->beRandrPending = FALSE);
		if (!RROutputSetClones (output, NULL, 0))
		    return (dmxScreen->beRandrPending = FALSE);
		if (!RROutputSetCrtcs (output, &pScrPriv->crtcs[baseCrtc], 1))
		    return (dmxScreen->beRandrPending = FALSE);
		if (!RROutputSetConnection (output, RR_Connected))
		    return (dmxScreen->beRandrPending = FALSE);
	    }
	    else
	    {
		if (!RROutputSetModes (output, NULL, 0, 0))
		    return (dmxScreen->beRandrPending = FALSE);
		if (!RROutputSetClones (output, NULL, 0))
		    return (dmxScreen->beRandrPending = FALSE);
		if (!RROutputSetCrtcs (output, NULL, 0))
		    return (dmxScreen->beRandrPending = FALSE);
		if (!RROutputSetConnection (output, RR_Disconnected))
		    return FALSE;
	    }
	}

	for (j = 0; j < crtcsPerScreen; j++)
	{
	    RRCrtcPtr crtc = pScrPriv->crtcs[baseCrtc + j];

	    if (r)
	    {
		if (j < r->ncrtc)
		{
		    if (!dmxRRUpdateCrtc (pScreen, dmxScreen, r, r->crtcs[j]))
			return (dmxScreen->beRandrPending = FALSE);
		}
		else
		{
		    RRCrtcNotify (crtc, NULL, 0, 0, RR_Rotate_0, 0, NULL);
		}
	    }
	    else if (dmxScreen->beDisplay && j == 0)
	    {
		RRModePtr   mode;
		xRRModeInfo modeInfo;
		char	    name[64];

		sprintf (name,
			 "%dx%d",
			 dmxScreen->scrnWidth, dmxScreen->scrnHeight);

		memset (&modeInfo, '\0', sizeof (modeInfo));
		modeInfo.width = dmxScreen->scrnWidth;
		modeInfo.height = dmxScreen->scrnHeight;
		modeInfo.nameLength = strlen (name);

		mode = RRModeGet (&modeInfo, name);
		if (!mode)
		    return (dmxScreen->beRandrPending = FALSE);

		RRCrtcNotify (crtc, mode,
			      dmxScreen->rootX, dmxScreen->rootY,
			      RR_Rotate_0, 1,
			      &pScrPriv->outputs[baseOutput]);

		if (mode)
		    RRModeDestroy (mode);
	    }
	    else
	    {
		RRCrtcNotify (crtc, NULL, 0, 0, RR_Rotate_0, 0, NULL);
	    }
	}

	if (r)
	    XRRFreeScreenResources (r);

	dmxScreen->beRandrPending = FALSE;
    }

    *rotations = RR_Rotate_0;

    for (i = 0; i < pScrPriv->numCrtcs; i++)
	*rotations |= pScrPriv->crtcs[i]->rotations;

    return TRUE;
}

static unsigned long
dmxRRGetXMode (XRRScreenResources *r,
	       RRModePtr	  mode)
{
    xRRModeInfo modeInfo = mode->mode;
    int		i;

    for (i = 0; i < r->nmode; i++)
    {
	if (modeInfo.width      == r->modes[i].width	  &&
	    modeInfo.height     == r->modes[i].height	  &&
	    modeInfo.dotClock   == r->modes[i].dotClock	  &&
	    modeInfo.hSyncStart == r->modes[i].hSyncStart &&
	    modeInfo.hSyncEnd   == r->modes[i].hSyncEnd	  &&
	    modeInfo.hTotal     == r->modes[i].hTotal	  &&
	    modeInfo.hSkew      == r->modes[i].hSkew	  &&
	    modeInfo.vSyncStart == r->modes[i].vSyncStart &&
	    modeInfo.vSyncEnd   == r->modes[i].vSyncEnd	  &&
	    modeInfo.vTotal     == r->modes[i].vTotal	  &&
	    modeInfo.nameLength == r->modes[i].nameLength &&
	    modeInfo.modeFlags  == r->modes[i].modeFlags)
	{
	    if (!memcmp (r->modes[i].name, mode->name, modeInfo.nameLength))
		return r->modes[i].id;
	}
    }

    return None;
}

static Bool
dmxRRScreenSetSize (ScreenPtr pScreen,
		    CARD16    width,
		    CARD16    height,
		    CARD32    mmWidth,
		    CARD32    mmHeight)
{

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	int i;

	for (i = 0; i < dmxNumScreens; i++)
	    dmxResizeRootWindow (WindowTable[i],
				 dmxScreens[i].rootX, dmxScreens[i].rootY,
				 width, height);

	for (i = 0; i < dmxNumScreens; i++)
	    dmxUpdateScreenResources (screenInfo.screens[i],
				      0, 0, width, height);

	dmxSetWidthHeight (width, height);
	XineramaReinitData (pScreen);
        dmxConnectionBlockCallback ();
    }
    else
#endif
    {
	dmxResizeRootWindow (WindowTable[pScreen->myNum],
			     dmxScreens[pScreen->myNum].rootX,
			     dmxScreens[pScreen->myNum].rootY,
			     width, height);
	dmxUpdateScreenResources (pScreen, 0, 0, width, height);
    }

    pScreen->mmWidth  = mmWidth;
    pScreen->mmHeight = mmHeight;

    RRScreenSizeNotify (pScreen);

    return TRUE;
}

static Bool
dmxRRCrtcSet (ScreenPtr   pScreen,
	      RRCrtcPtr   crtc,
	      RRModePtr   mode,
	      int	  x,
	      int	  y,
	      Rotation    rotation,
	      int	  numOutputs,
	      RROutputPtr *outputs)
{
    XRRScreenResources *r = NULL;

#ifdef _XSERVER64
    RROutput64	       *o = NULL;
#else
    RROutput	       *o = NULL;
#endif

    RRMode	       m = None;
    Status	       status = !RRSetConfigSuccess;
    int		       i;
    DMXScreenInfo      *dmxScreen;

    dmxScreen = dmxRRGetScreenForCrtc (pScreen, crtc);
    if (!dmxScreen)
	return FALSE;

    if (dmxScreen->beRandrPending)
	return RRCrtcNotify (crtc, mode, x, y, rotation, numOutputs, outputs);

    for (i = 0; i < numOutputs; i++)
	if (!dmxRRGetOutput (pScreen,
			     dmxScreen,
			     (unsigned long) outputs[i]->devPrivate))
	    return FALSE;

    if (numOutputs)
    {
	o = xalloc (sizeof (*o) * numOutputs);
	if (!o)
	    return FALSE;
    }

    XLIB_PROLOGUE (dmxScreen);
    r = XRRGetScreenResources (dmxScreen->beDisplay,
			       DefaultRootWindow (dmxScreen->beDisplay));
    XLIB_EPILOGUE (dmxScreen);

    if (!r)
	return FALSE;

    if (mode)
    {
	m = dmxRRGetXMode (r, mode);
	if (!m)
	{
	    XRRFreeScreenResources (r);
	    if (o)
		xfree (o);

	    return FALSE;
	}
    }

    for (i = 0; i < numOutputs; i++)
	o[i] = (unsigned long) outputs[i]->devPrivate;

    XLIB_PROLOGUE (dmxScreen);
    status = XRRSetCrtcConfig (dmxScreen->beDisplay, r,
			       (unsigned long) crtc->devPrivate,
			       CurrentTime,
			       x, y,
			       m,
			       rotation,
			       o, numOutputs);
    XLIB_EPILOGUE (dmxScreen);

    XRRFreeScreenResources (r);

    if (o)
	xfree (o);

    if (status != RRSetConfigSuccess)
	return FALSE;

    return RRCrtcNotify (crtc, mode, x, y, rotation, numOutputs, outputs);
}

static Bool
dmxRRCrtcSetGamma (ScreenPtr pScreen,
		   RRCrtcPtr crtc)
{
    XRRCrtcGamma  *gamma;
    DMXScreenInfo *dmxScreen;

    dmxScreen = dmxRRGetScreenForCrtc (pScreen, crtc);
    if (!dmxScreen)
	return FALSE;

    if (dmxScreen->beRandrPending)
	return TRUE;

    gamma = XRRAllocGamma (crtc->gammaSize);
    if (!gamma)
	return FALSE;

    memcpy (gamma->red,   crtc->gammaRed,   gamma->size * sizeof (CARD16));
    memcpy (gamma->green, crtc->gammaGreen, gamma->size * sizeof (CARD16));
    memcpy (gamma->blue,  crtc->gammaBlue,  gamma->size * sizeof (CARD16));

    XLIB_PROLOGUE (dmxScreen);
    XRRSetCrtcGamma (dmxScreen->beDisplay, (unsigned long) crtc->devPrivate,
		     gamma);
    XLIB_EPILOGUE (dmxScreen);

    XRRFreeGamma (gamma);

    return TRUE;
}

static Bool
dmxRROutputSetProperty (ScreenPtr	   pScreen,
			RROutputPtr	   output,
			Atom	           property,
			RRPropertyValuePtr value)
{
    RRPropertyPtr p;

#ifdef _XSERVER64
    Atom64	  atom = 0, type = 0;
#else
    Atom	  atom = 0, type = 0;
#endif

    long	  *values = value->data;
    long	  *validValues;
    int		  i;
    DMXScreenInfo *dmxScreen;

    dmxScreen = dmxRRGetScreenForOutput (pScreen, output);
    if (!dmxScreen)
	return FALSE;

    if (dmxScreen->beRandrPending)
	return TRUE;

    p = RRQueryOutputProperty (output, property);
    if (!p)
	return FALSE;

    validValues = p->valid_values;

    atom = dmxBEAtom (dmxScreen, property);
    type = dmxBEAtom (dmxScreen, value->type);

    if (type == XA_ATOM && value->format == 32)
    {
	INT32 *atoms = (INT32 *) value->data;

	for (i = 0; i < value->size; i++)
	    if (!ValidAtom (atoms[i]))
		return FALSE;

	if (p->num_valid > 0)
	{
	    for (i = 0; i < p->num_valid; i++)
		if (!ValidAtom (p->valid_values[i]))
		    return FALSE;

	    for (i = 0; i < value->size; i++)
	    {
		int j;

		for (j = 0; j < p->num_valid; j++)
		    if (p->valid_values[j] == atoms[i])
			break;

		if (j == p->num_valid)
		    return FALSE;
	    }

	    validValues = xalloc (p->num_valid * sizeof (long));
	    if (!validValues)
		return FALSE;

	    for (i = 0; i < p->num_valid; i++)
		validValues[i] = dmxBEAtom (dmxScreen, p->valid_values[i]);
	}

	if (value->size)
	{
	    int size = value->size * (value->format / 8);

	    values = xalloc (size);
	    if (!values)
		return FALSE;

	    for (i = 0; i < value->size; i++)
		values[i] = dmxBEAtom (dmxScreen, atoms[i]);
	}
    }
    else
    {
	if (p->num_valid > 0)
	{
	    validValues = xalloc (p->num_valid * sizeof (long));
	    if (!validValues)
		return FALSE;

	    for (i = 0; i < p->num_valid; i++)
		validValues[i] = p->valid_values[i];
	}
    }

    XLIB_PROLOGUE (dmxScreen);
    XRRConfigureOutputProperty (dmxScreen->beDisplay,
				(unsigned long) output->devPrivate,
				atom, p->is_pending, p->range, p->num_valid,
				validValues);
    XRRChangeOutputProperty (dmxScreen->beDisplay,
			     (unsigned long) output->devPrivate,
			     atom, type, value->format, PropModeReplace,
			     (unsigned char *) values, value->size);
    XLIB_EPILOGUE (dmxScreen);

    if (validValues != p->valid_values)
	xfree (validValues);

    if (values != value->data)
	xfree (values);

    return TRUE;
}

static Bool
dmxRROutputValidateMode (ScreenPtr   pScreen,
			 RROutputPtr output,
			 RRModePtr   mode)
{
    XRRModeInfo   *modeInfo;

#ifdef _XSERVER64
    RRMode64      m = 0;
#else
    RRMode        m = 0;
#endif

    DMXScreenInfo *dmxScreen;

    dmxScreen = dmxRRGetScreenForOutput (pScreen, output);
    if (!dmxScreen)
	return FALSE;

    if (dmxScreen->beRandrPending)
	return TRUE;

    modeInfo = XRRAllocModeInfo (mode->name, mode->mode.nameLength);
    if (!modeInfo)
	return FALSE;

    modeInfo->width      = mode->mode.width;
    modeInfo->height     = mode->mode.height;
    modeInfo->dotClock   = mode->mode.dotClock;
    modeInfo->hSyncStart = mode->mode.hSyncStart;
    modeInfo->hSyncEnd   = mode->mode.hSyncEnd;
    modeInfo->hTotal     = mode->mode.hTotal;
    modeInfo->hSkew      = mode->mode.hSkew;
    modeInfo->vSyncStart = mode->mode.vSyncStart;
    modeInfo->vSyncEnd   = mode->mode.vSyncEnd;
    modeInfo->vTotal     = mode->mode.vTotal;
    modeInfo->modeFlags  = mode->mode.modeFlags;

    XLIB_PROLOGUE (dmxScreen);
    m = XRRCreateMode (dmxScreen->beDisplay,
		       DefaultRootWindow (dmxScreen->beDisplay),
		       modeInfo);
    XLIB_EPILOGUE (dmxScreen);

    if (!m)
	return FALSE;

    XRRFreeModeInfo (modeInfo);

    XLIB_PROLOGUE (dmxScreen);
    XRRAddOutputMode (dmxScreen->beDisplay,
		      (unsigned long) output->devPrivate, m);
    XLIB_EPILOGUE (dmxScreen);

    return TRUE;
}

static void
dmxRRModeDestroy (ScreenPtr pScreen,
		  RRModePtr mode)
{
    XRRScreenResources *r;
    int		       i;

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (!dmxScreen->beRandr)
	    continue;

	if (dmxScreen->beRandrPending)
	    continue;

	r = NULL;

	XLIB_PROLOGUE (dmxScreen);
	r = XRRGetScreenResources (dmxScreen->beDisplay, dmxScreen->scrnWin);
	XLIB_EPILOGUE (dmxScreen);

	if (r)
	{

#ifdef _XSERVER64
	    RRMode64 m;
#else
	    RRMode   m;
#endif

	    m = dmxRRGetXMode (r, mode);
	    if (m)
	    {
		XLIB_PROLOGUE (dmxScreen);
		XRRDestroyMode (dmxScreen->beDisplay, m);
		XLIB_EPILOGUE (dmxScreen);
	    }

	    XRRFreeScreenResources (r);
	}
    }
}

Bool
dmxScreenEventCheckRR (ScreenPtr           pScreen,
		       xcb_generic_event_t *event)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    switch (event->response_type & ~0x80) {
    case XCB_MAP_NOTIFY:
	if (((xcb_map_notify_event_t *) event)->window == dmxScreen->rootWin)
	    return TRUE;

	return FALSE;
    case XCB_CONFIGURE_NOTIFY: {
	xcb_configure_notify_event_t *xconfigure =
	    (xcb_configure_notify_event_t *) event;
	XEvent                       X;

	if (xconfigure->window == dmxScreen->scrnWin)
	{
	    if (dmxScreen->scrnWidth  == xconfigure->width &&
		dmxScreen->scrnHeight == xconfigure->height)
		return TRUE;

	    dmxScreen->scrnWidth  = xconfigure->width;
	    dmxScreen->scrnHeight = xconfigure->height;
	}
	else if (xconfigure->window == dmxScreen->rootWin)
	{
	    if (dmxScreen->rootX == xconfigure->x &&
		dmxScreen->rootY == xconfigure->y)
		return TRUE;

	    dmxScreen->rootX = xconfigure->x;
	    dmxScreen->rootY = xconfigure->y;
	}
	else
	{
	    return FALSE;
	}

	X.xconfigure.type    = XCB_CONFIGURE_NOTIFY;
	X.xconfigure.display = dmxScreen->beDisplay;
	X.xconfigure.window  = xconfigure->window;
	X.xconfigure.width   = xconfigure->width;
	X.xconfigure.height  = xconfigure->height;

	XRRUpdateConfiguration (&X);
    } break;
    default:
	if (!dmxScreen->beRandr)
	    return FALSE;

	switch ((event->response_type & ~0x80) - dmxScreen->beRandrEventBase) {
	case XCB_RANDR_SCREEN_CHANGE_NOTIFY: {
	    xcb_randr_screen_change_notify_event_t *scevent =
		(xcb_randr_screen_change_notify_event_t *) event;
	    XRRScreenChangeNotifyEvent             X;

	    X.type           = event->response_type;
	    X.display        = dmxScreen->beDisplay;
	    X.root           = scevent->root;
	    X.width          = scevent->width;
	    X.mwidth         = scevent->mwidth;
	    X.height         = scevent->height;
	    X.mheight        = scevent->mheight;
	    X.rotation       = scevent->rotation;
	    X.subpixel_order = scevent->subpixel_order;
	    
	    XRRUpdateConfiguration ((XEvent *) &X);
	} break;
	case XCB_RANDR_NOTIFY:
	    break;
	default:
	    return FALSE;
	}
    }

    dmxScreen->beWidth =
	DisplayWidth (dmxScreen->beDisplay,
		      DefaultScreen (dmxScreen->beDisplay));
    dmxScreen->beHeight =
	DisplayHeight (dmxScreen->beDisplay,
		       DefaultScreen (dmxScreen->beDisplay));

    /* only call RRGetInfo when server is fully initialized */
    if (dmxScreens[0].inputOverlayWid)
	RRGetInfo (screenInfo.screens[0]);

    return TRUE;
}

Bool
dmxRRScreenInit (ScreenPtr pScreen)
{
    rrScrPrivPtr pScrPriv;

    if (!RRScreenInit (pScreen))
	return FALSE;

    pScrPriv = rrGetScrPriv (pScreen);
    pScrPriv->rrGetInfo            = dmxRRGetInfo;
    pScrPriv->rrScreenSetSize      = dmxRRScreenSetSize;
    pScrPriv->rrCrtcSet	           = dmxRRCrtcSet;
    pScrPriv->rrCrtcSetGamma       = dmxRRCrtcSetGamma;
    pScrPriv->rrOutputSetProperty  = dmxRROutputSetProperty;
    pScrPriv->rrOutputValidateMode = dmxRROutputValidateMode;
    pScrPriv->rrModeDestroy	   = dmxRRModeDestroy;

    RRScreenSetSizeRange (pScreen, 1, 1, SHRT_MAX, SHRT_MAX);

    if (pScreen->myNum)
    {
	char name[64];
	int  i;

	for (i = 0; i < xRROutputsPerScreen; i++)
	{
	    sprintf (name,
		     "dmx%d",
		     (pScreen->myNum - 1) * xRROutputsPerScreen + i);

	    if (!RROutputCreate (screenInfo.screens[0],
				 name,
				 strlen (name),
				 NULL))
		return FALSE;
	}

	for (i = 0; i < xRRCrtcsPerScreen; i++)
	    if (!RRCrtcCreate (screenInfo.screens[0], NULL))
		return FALSE;
    }
    else
    {
	XRRScreenResources *r = NULL;
	DMXScreenInfo      *dmxScreen = dmxScreens;
	Display            *display = dmxScreen->beDisplay;
	Bool               beRandr = FALSE;

	if (display && dmxScreen->scrnWin == DefaultRootWindow (display))
	{
	    int major, minor, status = 0;

	    XLIB_PROLOGUE (dmxScreen);
	    status = XRRQueryVersion (display, &major, &minor);
	    XLIB_EPILOGUE (dmxScreen);

	    if (status)
	    {
		if (major > 1 || (major == 1 && minor >= 2))
		{
		    int ignore;

		    XLIB_PROLOGUE (dmxScreen);
		    beRandr = XRRQueryExtension (display, &ignore, &ignore);
		    XLIB_EPILOGUE (dmxScreen);
		}
	    }
	}

	if (display && beRandr)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    r = XRRGetScreenResources (display, DefaultRootWindow (display));
	    XLIB_EPILOGUE (dmxScreen);
	}

	if (r)
	{
	    int i;

	    xRROutputsForFirstScreen = r->noutput;
	    xRRCrtcsForFirstScreen   = r->ncrtc;

	    for (i = 0; i < r->noutput; i++)
	    {
		XRROutputInfo *o;

		o = XRRGetOutputInfo (display, r, r->outputs[i]);
		if (!o)
		    return FALSE;

		if (!RROutputCreate (screenInfo.screens[0],
				     o->name, strlen (o->name),
				     NULL))
		    return FALSE;
	    }

	    for (i = 0; i < r->ncrtc; i++)
		if (!RRCrtcCreate (screenInfo.screens[0], NULL))
		    return FALSE;

	    XRRFreeScreenResources (r);
	}
	else
	{
	    if (!RROutputCreate (screenInfo.screens[0], "default", 7, NULL))
		return FALSE;

	    if (!RRCrtcCreate (screenInfo.screens[0], NULL))
		return FALSE;
	}
    }

    return TRUE;
}

Bool
dmxBERRScreenInit (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    dmxScreen->beRandr = FALSE;

    if (dmxScreen->scrnWin == DefaultRootWindow (dmxScreen->beDisplay))
    {
	int major, minor, status = 0;

	XLIB_PROLOGUE (dmxScreen);
	status = XRRQueryVersion (dmxScreen->beDisplay, &major, &minor);
	XLIB_EPILOGUE (dmxScreen);

	if (status)
	{
	    if (major > 1 || (major == 1 && minor >= 2))
	    {
		int ignore;

		XLIB_PROLOGUE (dmxScreen);
		dmxScreen->beRandr =
		    XRRQueryExtension (dmxScreen->beDisplay,
				       &dmxScreen->beRandrEventBase,
				       &ignore);
		XLIB_EPILOGUE (dmxScreen);

		dmxLog (dmxInfo, "RandR 1.2 is present\n");
	    }
	    else
	    {
		dmxLog (dmxInfo, "RandR 1.2 is not present\n");
	    }
	}
	else
	{
	    dmxLog (dmxInfo, "RandR extension missing\n");
	}
    }
	
    if (!dmxScreen->beRandr)
	return FALSE;

    XLIB_PROLOGUE (dmxScreen);
    XRRSelectInput (dmxScreen->beDisplay,
		    DefaultRootWindow (dmxScreen->beDisplay),
		    RRScreenChangeNotifyMask |
		    RRCrtcChangeNotifyMask   |
		    RROutputChangeNotifyMask |
		    RROutputPropertyNotifyMask);
    XLIB_EPILOGUE (dmxScreen);

    return TRUE;
}

void
dmxBERRScreenFini (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxScreen->beRandr)
    {
	XLIB_PROLOGUE (dmxScreen);
	XRRSelectInput (dmxScreen->beDisplay,
			DefaultRootWindow (dmxScreen->beDisplay),
			0);
	XLIB_EPILOGUE (dmxScreen);
    }
}


#endif
