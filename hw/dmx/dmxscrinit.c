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
 *   Kevin E. Martin <kem@redhat.com>
 *   David H. Dawes <dawes@xfree86.org>
 *
 */

/** \file
 * This file provides support for screen initialization. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxextension.h"
#include "dmxsync.h"
#include "dmxshadow.h"
#include "dmxscrinit.h"
#include "dmxcursor.h"
#include "dmxgc.h"
#include "dmxgcops.h"
#include "dmxwindow.h"
#include "dmxpixmap.h"
#include "dmxfont.h"
#include "dmxcmap.h"
#include "dmxprop.h"
#include "dmxdpms.h"
#include "dmxlog.h"
#include "dmxcb.h"

#ifdef RENDER
#include "dmxpict.h"
#endif

#ifdef RANDR
#include "randrstr.h"
#endif

#include "fb.h"
#include "mipointer.h"
#include "micmap.h"

extern Bool dmxCloseScreen(int idx, ScreenPtr pScreen);
static Bool dmxSaveScreen(ScreenPtr pScreen, int what);

static unsigned long dmxGeneration;
static unsigned long *dmxCursorGeneration;

static int dmxGCPrivateKeyIndex;
DevPrivateKey dmxGCPrivateKey = &dmxGCPrivateKey; /**< Private index for GCs       */
static int dmxWinPrivateKeyIndex;
DevPrivateKey dmxWinPrivateKey = &dmxWinPrivateKeyIndex; /**< Private index for Windows   */
static int dmxPixPrivateKeyIndex;
DevPrivateKey dmxPixPrivateKey = &dmxPixPrivateKeyIndex; /**< Private index for Pixmaps   */
int dmxFontPrivateIndex;        /**< Private index for Fonts     */
static int dmxScreenPrivateKeyIndex;
DevPrivateKey dmxScreenPrivateKey = &dmxScreenPrivateKeyIndex; /**< Private index for Screens   */
static int dmxColormapPrivateKeyIndex;
DevPrivateKey dmxColormapPrivateKey = &dmxColormapPrivateKeyIndex; /**< Private index for Colormaps */
#ifdef RENDER
static int dmxPictPrivateKeyIndex;
DevPrivateKey dmxPictPrivateKey = &dmxPictPrivateKeyIndex; /**< Private index for Picts     */
static int dmxGlyphSetPrivateKeyIndex;
DevPrivateKey dmxGlyphSetPrivateKey = &dmxGlyphSetPrivateKeyIndex; /**< Private index for GlyphSets */
#endif

#ifdef RANDR
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
	return FALSE;

    RRCrtcGammaSet (crtc, gamma->red, gamma->green, gamma->blue);

    XRRFreeGamma (gamma);

    RRCrtcNotify (crtc, mode, c->x, c->y, c->rotation, noutput, outputs);

    if (outputs)
	xfree (outputs);

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
    char	    *name = NULL;
    INT32           *values = NULL;

    output = dmxRRGetOutput (pScreen, dmxScreen, xoutput);
    if (!output)
	return TRUE; /* do nothing if the output doesn't exist */

    XLIB_PROLOGUE (dmxScreen);
    name = XGetAtomName (dmxScreen->beDisplay, xproperty);
    XLIB_EPILOGUE (dmxScreen);

    if (!name)
	return FALSE;

    atom = MakeAtom (name, strlen (name), TRUE);

    XFree (name);

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
	{
	    name = NULL;

	    XLIB_PROLOGUE (dmxScreen);
	    name = XGetAtomName (dmxScreen->beDisplay, atoms[i]);
	    XLIB_EPILOGUE (dmxScreen);

	    if (!name)
		return FALSE;

	    atoms[i] = MakeAtom (name, strlen (name), TRUE);

	    XFree (name);
	}

	if (!info->range && info->num_values > 0)
	{
	    for (i = 0; i < info->num_values; i++)
	    {
		name = NULL;

		XLIB_PROLOGUE (dmxScreen);
		name = XGetAtomName (dmxScreen->beDisplay, values[i]);
		XLIB_EPILOGUE (dmxScreen);

		if (!name)
		    return FALSE;

		values[i] = MakeAtom (name, strlen (name), TRUE);

		XFree (name);
	    }
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
				       DefaultRootWindow (dmxScreen->beDisplay));
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
			 dmxScreen->beWidth, dmxScreen->beHeight);

		memset (&modeInfo, '\0', sizeof (modeInfo));
		modeInfo.width = dmxScreen->beWidth;
		modeInfo.height = dmxScreen->beHeight;
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
			 dmxScreen->beWidth, dmxScreen->beHeight);

		memset (&modeInfo, '\0', sizeof (modeInfo));
		modeInfo.width = dmxScreen->beWidth;
		modeInfo.height = dmxScreen->beHeight;
		modeInfo.nameLength = strlen (name);

		mode = RRModeGet (&modeInfo, name);
		if (!mode)
		    return (dmxScreen->beRandrPending = FALSE);

		RRCrtcNotify (crtc, mode, 0, 0, RR_Rotate_0, 1,
			      &pScrPriv->outputs[baseOutput]);
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
    DMXDesktopAttributesRec attr;

    dmxGetDesktopAttributes (&attr);

    if (attr.width != width || attr.height != height)
    {
	attr.width  = width;
	attr.height = height;

	if (dmxConfigureDesktop (&attr) != Success)
	    return FALSE;
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

    XLIB_PROLOGUE (dmxScreen);
    atom = XInternAtom (dmxScreen->beDisplay, NameForAtom (property), FALSE);
    type = XInternAtom (dmxScreen->beDisplay, NameForAtom (value->type),
			FALSE);
    XLIB_EPILOGUE (dmxScreen);

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
	    {
		XLIB_PROLOGUE (dmxScreen);
		validValues[i] = XInternAtom (dmxScreen->beDisplay,
					      NameForAtom (p->valid_values[i]),
					      FALSE);
		XLIB_EPILOGUE (dmxScreen);
	    }
	}

	if (value->size)
	{
	    int size = value->size * (value->format / 8);

	    values = xalloc (size);
	    if (!values)
		return FALSE;

	    for (i = 0; i < value->size; i++)
	    {
		XLIB_PROLOGUE (dmxScreen);
		values[i] = XInternAtom (dmxScreen->beDisplay,
					 NameForAtom (atoms[i]),
					 FALSE);
		XLIB_EPILOGUE (dmxScreen);
	    }
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
	r = XRRGetScreenResources (dmxScreen->beDisplay,
				   DefaultRootWindow (dmxScreen->beDisplay));
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

static void
dmxRRCheckScreen (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxScreen->beDisplay)
    {
	XEvent X;

	for (;;)
	{
	    Bool status = FALSE;

	    XLIB_PROLOGUE (dmxScreen);
	    status = XCheckTypedEvent (dmxScreen->beDisplay, ConfigureNotify, &X);
	    XLIB_EPILOGUE (dmxScreen);

	    if (!status)
		break;

	    XRRUpdateConfiguration (&X);

	    dmxScreen->beWidth =
		DisplayWidth (dmxScreen->beDisplay,
			      DefaultScreen (dmxScreen->beDisplay));
	    dmxScreen->beHeight =
		DisplayHeight (dmxScreen->beDisplay,
			       DefaultScreen (dmxScreen->beDisplay));

	    if (dmxScreen->beRandr)
	    {
		DMXScreenAttributesRec attr;
		CARD32		       scrnNum = dmxScreen->index;

		memset (&attr, 0, sizeof (attr));

		attr.screenWindowWidth  = dmxGlobalWidth;
		attr.screenWindowHeight = dmxGlobalHeight;

		if (attr.screenWindowWidth > dmxScreen->beWidth)
		    attr.screenWindowWidth = dmxScreen->beWidth;
		if (attr.screenWindowHeight > dmxScreen->beHeight)
		    attr.screenWindowHeight = dmxScreen->beHeight;

		attr.rootWindowWidth  = attr.screenWindowWidth;
		attr.rootWindowHeight = attr.screenWindowHeight;

		dmxConfigureScreenWindows (1, &scrnNum, &attr, NULL);
	    }

	    RRTellChanged (screenInfo.screens[0]);
	}

	if (dmxScreen->beRandr)
	{
	    for (;;)
	    {
		Bool status = FALSE;

		XLIB_PROLOGUE (dmxScreen);
		status = XCheckTypedEvent (dmxScreen->beDisplay,
					   dmxScreen->beRandrEventBase +
					   RRScreenChangeNotify,
					   &X);
		XLIB_EPILOGUE (dmxScreen);

		if (!status)
		    break;

		XRRUpdateConfiguration (&X);
		RRTellChanged (screenInfo.screens[0]);
	    }

	    for (;;)
	    {
		Bool status = FALSE;

		XLIB_PROLOGUE (dmxScreen);
		status = XCheckTypedEvent (dmxScreen->beDisplay,
					   dmxScreen->beRandrEventBase + RRNotify,
					   &X);
		XLIB_EPILOGUE (dmxScreen);

		if (!status)
		    break;

		XRRUpdateConfiguration (&X);
		RRTellChanged (screenInfo.screens[0]);
	    }
	}
    }
}

static Bool
dmxRRInit (ScreenPtr pScreen)
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

	if (dmxScreens->beDisplay && dmxScreens->beRandr)
	{
	    XLIB_PROLOGUE (dmxScreens);
	    r = XRRGetScreenResources (dmxScreens->beDisplay,
				       DefaultRootWindow (dmxScreens->beDisplay));
	    XLIB_EPILOGUE (dmxScreens);
	}

	if (r)
	{
	    int i;

	    xRROutputsForFirstScreen = r->noutput;
	    xRRCrtcsForFirstScreen   = r->ncrtc;

	    for (i = 0; i < r->noutput; i++)
	    {
		XRROutputInfo *o;

		o = XRRGetOutputInfo (dmxScreens->beDisplay, r, r->outputs[i]);
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

#endif

/** Initialize the parts of screen \a idx that require access to the
 *  back-end server. */
void dmxBEScreenInit(int idx, ScreenPtr pScreen)
{
    DMXScreenInfo        *dmxScreen = &dmxScreens[idx];
    XSetWindowAttributes  attribs;
    XGCValues             gcvals;
    unsigned long         mask;
    int                   i, j;

    /* FIXME: The dmxScreenInit() code currently assumes that it will
     * not be called if the Xdmx server is started with this screen
     * detached -- i.e., it assumes that dmxScreen->beDisplay is always
     * valid.  This is not necessarily a valid assumption when full
     * addition/removal of screens is implemented, but when this code is
     * broken out for screen reattachment, then we will reevaluate this
     * assumption.
     */

    pScreen->mmWidth = DisplayWidthMM(dmxScreen->beDisplay, 
				      DefaultScreen(dmxScreen->beDisplay));
    pScreen->mmHeight = DisplayHeightMM(dmxScreen->beDisplay, 
					DefaultScreen(dmxScreen->beDisplay));

    pScreen->whitePixel = dmxScreen->beWhitePixel;
    pScreen->blackPixel = dmxScreen->beBlackPixel;

    /* Handle screen savers and DPMS on the backend */
    dmxDPMSInit(dmxScreen);

    /* Create root window for screen */
    mask = CWBackPixel | CWEventMask | CWColormap | CWOverrideRedirect;
    attribs.background_pixel = dmxScreen->beBlackPixel;
    attribs.event_mask = (KeyPressMask
                          | KeyReleaseMask
                          | ButtonPressMask
                          | ButtonReleaseMask
                          | EnterWindowMask
                          | LeaveWindowMask
                          | PointerMotionMask
                          | KeymapStateMask
                          | FocusChangeMask);
    attribs.colormap = dmxScreen->beDefColormaps[dmxScreen->beDefVisualIndex];
    attribs.override_redirect = True;

    
    dmxScreen->scrnWin =
	XCreateWindow(dmxScreen->beDisplay, 
		      DefaultRootWindow(dmxScreen->beDisplay),
		      dmxScreen->scrnX,
		      dmxScreen->scrnY,
		      dmxScreen->scrnWidth,
		      dmxScreen->scrnHeight,
		      0,
		      pScreen->rootDepth,
		      InputOutput,
		      dmxScreen->beVisuals[dmxScreen->beDefVisualIndex].visual,
		      mask,
		      &attribs);
    dmxPropertyWindow(dmxScreen);

    /*
     * This turns off the cursor by defining a cursor with no visible
     * components.
     */
    if (1) {
	char noCursorData[] = {0, 0, 0, 0,
			       0, 0, 0, 0};
	Pixmap pixmap;
	XColor color, tmp;

	pixmap = XCreateBitmapFromData(dmxScreen->beDisplay, dmxScreen->scrnWin,
				       noCursorData, 8, 8);
	XAllocNamedColor(dmxScreen->beDisplay, dmxScreen->beDefColormaps[0],
			 "black", &color, &tmp);
	dmxScreen->noCursor = XCreatePixmapCursor(dmxScreen->beDisplay,
						  pixmap, pixmap,
						  &color, &color, 0, 0);
	XDefineCursor(dmxScreen->beDisplay, dmxScreen->scrnWin,
		      dmxScreen->noCursor);

	XFreePixmap(dmxScreen->beDisplay, pixmap);
    }

#ifdef RANDR
    XSelectInput (dmxScreen->beDisplay,
		  DefaultRootWindow (dmxScreen->beDisplay),
		  StructureNotifyMask);

    if (dmxScreen->beRandr)
	XRRSelectInput (dmxScreen->beDisplay,
			DefaultRootWindow (dmxScreen->beDisplay),
			RRScreenChangeNotifyMask |
			RRCrtcChangeNotifyMask	 |
			RROutputChangeNotifyMask |
			RROutputPropertyNotifyMask);
#endif

    XMapWindow (dmxScreen->beDisplay, dmxScreen->scrnWin);

    XSetInputFocus (dmxScreen->beDisplay, dmxScreen->scrnWin,
		    RevertToPointerRoot,
		    CurrentTime);

    if (dmxShadowFB) {
	mask = (GCFunction
		| GCPlaneMask
		| GCClipMask);
	gcvals.function = GXcopy;
	gcvals.plane_mask = AllPlanes;
	gcvals.clip_mask = None;

	dmxScreen->shadowGC = XCreateGC(dmxScreen->beDisplay,
					dmxScreen->scrnWin,
					mask, &gcvals);

	dmxScreen->shadowFBImage =
	    XCreateImage(dmxScreen->beDisplay,
			 dmxScreen->beVisuals[dmxScreen->beDefVisualIndex].visual,
			 dmxScreen->beDepth,
			 ZPixmap,
			 0,
			 (char *)dmxScreen->shadow,
			 dmxScreen->scrnWidth, dmxScreen->scrnHeight,
			 dmxScreen->beBPP,
			 PixmapBytePad(dmxScreen->scrnWidth,
				       dmxScreen->beBPP));
    } else {
	/* Create default drawables (used during GC creation) */
	for (i = 0; i < dmxScreen->beNumPixmapFormats; i++) 
	    for (j = 0; j < dmxScreen->beNumDepths; j++)
		if ((dmxScreen->bePixmapFormats[i].depth == 1) ||
		    (dmxScreen->bePixmapFormats[i].depth ==
		     dmxScreen->beDepths[j])) {
		    dmxScreen->scrnDefDrawables[i] = (Drawable)
			XCreatePixmap(dmxScreen->beDisplay, dmxScreen->scrnWin,
				      1, 1, dmxScreen->bePixmapFormats[i].depth);
		    break;
		}
    }
}

static void
dmxSetWindowPixmap (WindowPtr pWin, PixmapPtr pPixmap)
{
    ScreenPtr       pScreen = pWin->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    PixmapPtr       pOld = (*pScreen->GetWindowPixmap) (pWin);

    if (pPixmap != pOld)
    {
	dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pWin);

	if (pPixmap)
	{
	    if ((*pScreen->GetWindowPixmap) (pWin->parent) != pPixmap)
	    {
		pWinPriv->redirected = TRUE;
	    }
	    else if (pWinPriv->redirected)
	    {
		if (dmxScreen->beDisplay)
		{
		    XLIB_PROLOGUE (dmxScreen);
		    XCompositeUnredirectWindow (dmxScreen->beDisplay,
						pWinPriv->window,
						CompositeRedirectManual);
		    XLIB_EPILOGUE (dmxScreen);
		}
		pWinPriv->redirected = FALSE;
	    }
	}
	else if (pWinPriv->redirected)
	{
	    if (dmxScreen->beDisplay)
	    {
		XLIB_PROLOGUE (dmxScreen);
		XCompositeUnredirectWindow (dmxScreen->beDisplay,
					    pWinPriv->window,
					    CompositeRedirectManual);
		XLIB_EPILOGUE (dmxScreen);
	    }
	    pWinPriv->redirected = FALSE;
	}
    }

    DMX_UNWRAP(SetWindowPixmap, dmxScreen, pScreen);
    if (pScreen->SetWindowPixmap)
	(*pScreen->SetWindowPixmap) (pWin, pPixmap);
    DMX_WRAP(SetWindowPixmap, dmxSetWindowPixmap, dmxScreen, pScreen);
}

static void
dmxScreenExpose (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxScreen->beDisplay)
    {
	WindowPtr pChild0, pChildN;
	XEvent    X;

	for (;;)
	{
	    Bool status = FALSE;

	    XLIB_PROLOGUE (dmxScreen);

	    status = XCheckTypedEvent (dmxScreen->beDisplay, Expose, &X);

	    XLIB_EPILOGUE (dmxScreen);

	    if (!status)
		break;

	    if (dmxShouldIgnore (dmxScreen, X.xexpose.serial))
		continue;

	    pChild0 = WindowTable[0];
	    pChildN = WindowTable[pScreen->myNum];

	    for (;;)
	    {
		dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pChildN);

		if (pWinPriv->window == X.xexpose.window)
		    break;

		if (pChild0->firstChild)
		{
		    pChild0 = pChild0->firstChild;
		    pChildN = pChildN->firstChild;
		    continue;
		}

		while (!pChild0->nextSib && (pChild0 != WindowTable[0]))
		{
		    pChild0 = pChild0->parent;
		    pChildN = pChildN->parent;
		}

		if (pChild0 == WindowTable[0])
		    break;

		pChild0 = pChild0->nextSib;
		pChildN = pChildN->nextSib;
	    }

	    if (pChild0)
	    {
		RegionRec region;
		BoxRec    box;

		box.x1 = pChild0->drawable.x + X.xexpose.x;
		box.y1 = pChild0->drawable.y + X.xexpose.y;
		box.x2 = box.x1 + X.xexpose.width;
		box.y2 = box.y1 + X.xexpose.height;

		REGION_INIT (screenInfo.screens[0], &region, &box, 1);
		(*pScreen->WindowExposures) (pChild0, &region, NullRegion);
		REGION_UNINIT (screenInfo.screens[0], &region);
	    }
	}
    }
}

static void
dmxScreenBlockHandler (pointer   blockData,
		       OSTimePtr pTimeout,
		       pointer   pReadMask)
{
    ScreenPtr pScreen = (ScreenPtr) blockData;

    dmxScreenExpose (pScreen);
}

static void
dmxScreenWakeupHandler (pointer blockData,
			int     result,
			pointer pReadMask)
{
    ScreenPtr pScreen = (ScreenPtr) blockData;

#ifdef RANDR
    dmxRRCheckScreen (pScreen);
#endif

}

/** Initialize screen number \a idx. */
Bool dmxScreenInit(int idx, ScreenPtr pScreen, int argc, char *argv[])
{
    DMXScreenInfo        *dmxScreen = &dmxScreens[idx];
    int                   i, j;

    if (dmxGeneration != serverGeneration) {
	/* Allocate font private index */
	dmxFontPrivateIndex = AllocateFontPrivateIndex();
	if (dmxFontPrivateIndex == -1)
	    return FALSE;

	dmxGeneration = serverGeneration;
    }

    dmxScreen->ignoreHead = NULL;
    dmxScreen->ignoreTail = &dmxScreen->ignoreHead;

#ifdef RANDR
    dmxScreen->beRandr = FALSE;
    dmxScreen->beRandrPending = FALSE;
#endif

    if (dmxShadowFB) {
	dmxScreen->shadow = shadowAlloc(dmxScreen->scrnWidth,
					dmxScreen->scrnHeight,
					dmxScreen->beBPP);
    } else {
	if (!dmxInitGC(pScreen)) return FALSE;
	if (!dmxInitWindow(pScreen)) return FALSE;
	if (!dmxInitPixmap(pScreen)) return FALSE;
    }

    /*
     * Initalise the visual types.  miSetVisualTypesAndMasks() requires
     * that all of the types for each depth be collected together.  It's
     * intended for slightly different usage to what we would like here.
     * Maybe a miAddVisualTypeAndMask() function will be added to make
     * things easier here.
     */
    if (dmxScreen->beDisplay)
    {
	for (i = 0; i < dmxScreen->beNumDepths; i++) {
	    int    depth;
	    int    visuals        = 0;
	    int    bitsPerRgb     = 0;
	    int    preferredClass = -1;
	    Pixel  redMask        = 0;
	    Pixel  greenMask      = 0;
	    Pixel  blueMask       = 0;

	    depth = dmxScreen->beDepths[i];
	    for (j = 0; j < dmxScreen->beNumVisuals; j++) {
		XVisualInfo *vi;

		vi = &dmxScreen->beVisuals[j];
		if (vi->depth == depth) {
		    /* Assume the masks are all the same. */
		    visuals |= (1 << vi->class);
		    bitsPerRgb = vi->bits_per_rgb;
		    redMask = vi->red_mask;
		    greenMask = vi->green_mask;
		    blueMask = vi->blue_mask;
		    if (j == dmxScreen->beDefVisualIndex) {
			preferredClass = vi->class;
		    }
		}
	    }
	    miSetVisualTypesAndMasks(depth, visuals, bitsPerRgb,
				     preferredClass,
				     redMask, greenMask, blueMask);
	}
    }
    else
    {
	for (i = 0; i < dmxScreens[0].beNumDepths; i++) {
	    int    depth;
	    int    visuals        = 0;
	    int    bitsPerRgb     = 0;
	    int    preferredClass = -1;
	    Pixel  redMask        = 0;
	    Pixel  greenMask      = 0;
	    Pixel  blueMask       = 0;

	    depth = dmxScreens[0].beDepths[i];
	    for (j = 0; j < dmxScreens[0].beNumVisuals; j++) {
		XVisualInfo *vi;

		vi = &dmxScreens[0].beVisuals[j];
		if (vi->depth == depth) {
		    /* Assume the masks are all the same. */
		    visuals |= (1 << vi->class);
		    bitsPerRgb = vi->bits_per_rgb;
		    redMask = vi->red_mask;
		    greenMask = vi->green_mask;
		    blueMask = vi->blue_mask;
		    if (j == dmxScreens[0].beDefVisualIndex) {
			preferredClass = vi->class;
		    }
		}
	    }
	    miSetVisualTypesAndMasks(depth, visuals, bitsPerRgb,
				     preferredClass,
				     redMask, greenMask, blueMask);
	}
    }

    fbScreenInit(pScreen,
		 dmxShadowFB ? dmxScreen->shadow : NULL,
		 dmxScreen->scrnWidth,
		 dmxScreen->scrnHeight,
		 dmxScreen->beXDPI,
		 dmxScreen->beYDPI,
		 dmxScreen->scrnWidth,
		 dmxScreen->beBPP);
#ifdef RENDER
    (void)dmxPictureInit(pScreen, 0, 0);
#endif

#ifdef RANDR
    if (dmxScreen->beDisplay)
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
	    }
	}
    }

    if (!dmxRRInit (pScreen))
	return FALSE;
#endif

    if (dmxShadowFB && !shadowInit(pScreen, dmxShadowUpdateProc, NULL))
	return FALSE;

    miInitializeBackingStore(pScreen);

    if (dmxShadowFB) {
	miDCInitialize(pScreen, &dmxPointerCursorFuncs);
    } else {
        MAXSCREENSALLOC(dmxCursorGeneration);
	if (dmxCursorGeneration[idx] != serverGeneration) {
	    if (!(miPointerInitialize(pScreen,
				      &dmxPointerSpriteFuncs,
				      &dmxPointerCursorFuncs,
				      FALSE)))
		return FALSE;

	    dmxCursorGeneration[idx] = serverGeneration;
	}
    }

    DMX_WRAP(CloseScreen, dmxCloseScreen, dmxScreen, pScreen);
    DMX_WRAP(SaveScreen, dmxSaveScreen, dmxScreen, pScreen);

    if (dmxScreen->beDisplay)
	dmxBEScreenInit(idx, pScreen);

    if (!dmxShadowFB) {
	/* Wrap GC functions */
	DMX_WRAP(CreateGC, dmxCreateGC, dmxScreen, pScreen);

	/* Wrap Window functions */
	DMX_WRAP(CreateWindow, dmxCreateWindow, dmxScreen, pScreen);
	DMX_WRAP(DestroyWindow, dmxDestroyWindow, dmxScreen, pScreen);
	DMX_WRAP(PositionWindow, dmxPositionWindow, dmxScreen, pScreen);
	DMX_WRAP(ChangeWindowAttributes, dmxChangeWindowAttributes, dmxScreen,
		 pScreen);
	DMX_WRAP(RealizeWindow, dmxRealizeWindow, dmxScreen, pScreen);
	DMX_WRAP(UnrealizeWindow, dmxUnrealizeWindow, dmxScreen, pScreen);
	DMX_WRAP(RestackWindow, dmxRestackWindow, dmxScreen, pScreen);
	DMX_WRAP(CopyWindow, dmxCopyWindow, dmxScreen, pScreen);

	DMX_WRAP(ResizeWindow, dmxResizeWindow, dmxScreen, pScreen);
	DMX_WRAP(ReparentWindow, dmxReparentWindow, dmxScreen, pScreen);

	DMX_WRAP(ChangeBorderWidth, dmxChangeBorderWidth, dmxScreen, pScreen);

	DMX_WRAP(SetWindowPixmap, dmxSetWindowPixmap, dmxScreen, pScreen);

	/* Wrap Image functions */
	DMX_WRAP(GetImage, dmxGetImage, dmxScreen, pScreen);
	DMX_WRAP(GetSpans, NULL, dmxScreen, pScreen);

	/* Wrap Pixmap functions */
	DMX_WRAP(CreatePixmap, dmxCreatePixmap, dmxScreen, pScreen);
	DMX_WRAP(DestroyPixmap, dmxDestroyPixmap, dmxScreen, pScreen);
	DMX_WRAP(BitmapToRegion, dmxBitmapToRegion, dmxScreen, pScreen);

	/* Wrap Font functions */
	DMX_WRAP(RealizeFont, dmxRealizeFont, dmxScreen, pScreen);
	DMX_WRAP(UnrealizeFont, dmxUnrealizeFont, dmxScreen, pScreen);

	/* Wrap Colormap functions */
	DMX_WRAP(CreateColormap, dmxCreateColormap, dmxScreen, pScreen);
	DMX_WRAP(DestroyColormap, dmxDestroyColormap, dmxScreen, pScreen);
	DMX_WRAP(InstallColormap, dmxInstallColormap, dmxScreen, pScreen);
	DMX_WRAP(StoreColors, dmxStoreColors, dmxScreen, pScreen);

	/* Wrap Shape functions */
	DMX_WRAP(SetShape, dmxSetShape, dmxScreen, pScreen);
    }

    if (!dmxCreateDefColormap(pScreen))
	return FALSE;

    RegisterBlockAndWakeupHandlers (dmxScreenBlockHandler,
				    dmxScreenWakeupHandler,
				    pScreen);

    return TRUE;
}

/** Close the \a pScreen resources on the back-end server. */
void dmxBECloseScreen(ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    int            i;

    /* Restore the back-end screen-saver and DPMS state. */
    dmxDPMSTerm(dmxScreen);

    /* Free the screen resources */

    XLIB_PROLOGUE (dmxScreen);
    XFreeCursor(dmxScreen->beDisplay, dmxScreen->noCursor);
    XLIB_EPILOGUE (dmxScreen);
    dmxScreen->noCursor = (Cursor)0;

    XLIB_PROLOGUE (dmxScreen);
    XUnmapWindow(dmxScreen->beDisplay, dmxScreen->scrnWin);
    XDestroyWindow(dmxScreen->beDisplay, dmxScreen->scrnWin);
    XLIB_EPILOGUE (dmxScreen);
    dmxScreen->scrnWin = (Window)0;

    if (dmxShadowFB) {
	/* Free the shadow GC and image assocated with the back-end server */
	XLIB_PROLOGUE (dmxScreen);
	XFreeGC(dmxScreen->beDisplay, dmxScreen->shadowGC);
	XLIB_EPILOGUE (dmxScreen);
	dmxScreen->shadowGC = NULL;
	XFree(dmxScreen->shadowFBImage);
	dmxScreen->shadowFBImage = NULL;
    } else {
	/* Free the default drawables */
	for (i = 0; i < dmxScreen->beNumPixmapFormats; i++) {
	    XLIB_PROLOGUE (dmxScreen);
	    XFreePixmap(dmxScreen->beDisplay, dmxScreen->scrnDefDrawables[i]);
	    XLIB_EPILOGUE (dmxScreen);
	    dmxScreen->scrnDefDrawables[i] = (Drawable)0;
	}
    }

    /* Free resources allocated during initialization (in dmxinit.c) */
    for (i = 0; i < dmxScreen->beNumDefColormaps; i++)
    {
	XLIB_PROLOGUE (dmxScreen);
	XFreeColormap(dmxScreen->beDisplay, dmxScreen->beDefColormaps[i]);
	XLIB_EPILOGUE (dmxScreen);
    }
    xfree(dmxScreen->beDefColormaps);
    dmxScreen->beDefColormaps = NULL;

#if 0
    /* Do not free visuals, depths and pixmap formats here.  Free them
     * in dmxCloseScreen() instead -- see comment below. */
    XFree(dmxScreen->beVisuals);
    dmxScreen->beVisuals = NULL;

    XFree(dmxScreen->beDepths);
    dmxScreen->beDepths = NULL;

    XFree(dmxScreen->bePixmapFormats);
    dmxScreen->bePixmapFormats = NULL;
#endif

#ifdef GLXEXT
    if (dmxScreen->glxVisuals) {
	XFree(dmxScreen->glxVisuals);
	dmxScreen->glxVisuals = NULL;
	dmxScreen->numGlxVisuals = 0;
    }
#endif

    /* Close display */
    XLIB_PROLOGUE (dmxScreen);
    XCloseDisplay(dmxScreen->beDisplay);
    XLIB_EPILOGUE (dmxScreen);
    dmxScreen->beDisplay = NULL;
}

/** Close screen number \a idx. */
Bool dmxCloseScreen(int idx, ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[idx];

    /* Reset the proc vectors */
    if (idx == 0) {
#ifdef RENDER
	dmxResetRender();
#endif
	dmxResetFonts();
    }

    if (dmxShadowFB) {
	/* Free the shadow framebuffer */
	xfree(dmxScreen->shadow);
    } else {

	/* Unwrap Shape functions */
	DMX_UNWRAP(SetShape, dmxScreen, pScreen);

	/* Unwrap the pScreen functions */
	DMX_UNWRAP(CreateGC, dmxScreen, pScreen);

	DMX_UNWRAP(CreateWindow, dmxScreen, pScreen);
	DMX_UNWRAP(DestroyWindow, dmxScreen, pScreen);
	DMX_UNWRAP(PositionWindow, dmxScreen, pScreen);
	DMX_UNWRAP(ChangeWindowAttributes, dmxScreen, pScreen);
	DMX_UNWRAP(RealizeWindow, dmxScreen, pScreen);
	DMX_UNWRAP(UnrealizeWindow, dmxScreen, pScreen);
	DMX_UNWRAP(RestackWindow, dmxScreen, pScreen);
	DMX_UNWRAP(CopyWindow, dmxScreen, pScreen);

	DMX_UNWRAP(ResizeWindow, dmxScreen, pScreen);
	DMX_UNWRAP(ReparentWindow, dmxScreen, pScreen);

	DMX_UNWRAP(ChangeBorderWidth, dmxScreen, pScreen);

	DMX_UNWRAP(GetImage, dmxScreen, pScreen);
	DMX_UNWRAP(GetSpans, dmxScreen, pScreen);

	DMX_UNWRAP(CreatePixmap, dmxScreen, pScreen);
	DMX_UNWRAP(DestroyPixmap, dmxScreen, pScreen);
	DMX_UNWRAP(BitmapToRegion, dmxScreen, pScreen);

	DMX_UNWRAP(RealizeFont, dmxScreen, pScreen);
	DMX_UNWRAP(UnrealizeFont, dmxScreen, pScreen);

	DMX_UNWRAP(CreateColormap, dmxScreen, pScreen);
	DMX_UNWRAP(DestroyColormap, dmxScreen, pScreen);
	DMX_UNWRAP(InstallColormap, dmxScreen, pScreen);
	DMX_UNWRAP(StoreColors, dmxScreen, pScreen);
    }

    DMX_UNWRAP(SaveScreen, dmxScreen, pScreen);

    if (dmxScreen->beDisplay) {
	dmxBECloseScreen(pScreen);

#if 1
	/* Free visuals, depths and pixmap formats here so that they
	 * won't be freed when a screen is detached, thereby allowing
	 * the screen to be reattached to be compared to the one
	 * previously removed.
	 */
	XFree(dmxScreen->beVisuals);
	dmxScreen->beVisuals = NULL;

	XFree(dmxScreen->beDepths);
	dmxScreen->beDepths = NULL;

	XFree(dmxScreen->bePixmapFormats);
	dmxScreen->bePixmapFormats = NULL;
#endif
    }

    DMX_UNWRAP(CloseScreen, dmxScreen, pScreen);
    return pScreen->CloseScreen(idx, pScreen);
}

static Bool dmxSaveScreen(ScreenPtr pScreen, int what)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxScreen->beDisplay) {
	switch (what) {
	case SCREEN_SAVER_OFF:
	case SCREEN_SAVER_FORCER:
	    XLIB_PROLOGUE (dmxScreen);
	    XResetScreenSaver(dmxScreen->beDisplay);
	    XLIB_EPILOGUE (dmxScreen);
	    dmxSync(dmxScreen, FALSE);
	    break;
	case SCREEN_SAVER_ON:
	case SCREEN_SAVER_CYCLE:
	    XLIB_PROLOGUE (dmxScreen);
	    XActivateScreenSaver(dmxScreen->beDisplay);
	    XLIB_EPILOGUE (dmxScreen);
	    dmxSync(dmxScreen, FALSE);
	    break;
	}
    }

    return TRUE;
}

void
dmxDiscardIgnore (DMXScreenInfo *dmxScreen,
		  unsigned long sequence)
{
    while (dmxScreen->ignoreHead)
    {
	if ((long) (sequence - dmxScreen->ignoreHead->sequence) > 0)
	{
	    DMXIgnore *next = dmxScreen->ignoreHead->next;

	    free (dmxScreen->ignoreHead);

	    dmxScreen->ignoreHead = next;
	    if (!dmxScreen->ignoreHead)
		dmxScreen->ignoreTail = &dmxScreen->ignoreHead;
	}
	else
	    break;
    }
}

void
dmxSetIgnore (DMXScreenInfo *dmxScreen,
	      unsigned long sequence)
{
    DMXIgnore *i;

    i = malloc (sizeof (DMXIgnore));
    if (!i)
	return;

    i->sequence = sequence;
    i->next     = 0;

    *(dmxScreen->ignoreTail) = i;
    dmxScreen->ignoreTail = &i->next;
}

Bool
dmxShouldIgnore (DMXScreenInfo *dmxScreen,
		 unsigned long sequence)
{
    dmxDiscardIgnore (dmxScreen, sequence);

    if (!dmxScreen->ignoreHead)
	return FALSE;

    return dmxScreen->ignoreHead->sequence == sequence;
}
