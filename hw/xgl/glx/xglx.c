/*
 * Copyright © 2004 David Reveman
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * David Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * David Reveman makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include "xglx.h"

#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>
#include <X11/cursorfont.h>

#include <glitz-glx.h>

#ifdef GLXEXT
#include "xglglxext.h"
#endif

#include "inputstr.h"
#include "cursorstr.h"
#include "mipointer.h"

#ifdef RANDR
#include "randrstr.h"
#endif

#ifdef PANORAMIX
#include <X11/extensions/panoramiXproto.h>
#include "extnsionst.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#ifdef XKB
#include <X11/extensions/XKB.h>
#include <X11/extensions/XKBsrv.h>
#include <X11/extensions/XKBconfig.h>
#include <X11/extensions/XKBrules.h>

extern Bool
XkbQueryExtension (Display *dpy,
		   int     *opcodeReturn,
		   int     *eventBaseReturn,
		   int     *errorBaseReturn,
		   int     *majorRtrn,
		   int     *minorRtrn);

extern XkbDescPtr
XkbGetKeyboard (Display      *dpy,
		unsigned int which,
		unsigned int deviceSpec);

extern Status
XkbGetControls (Display	    *dpy,
		unsigned long which,
		XkbDescPtr    desc);

#ifndef XKB_DFLT_RULES_FILE
#define	XKB_DFLT_RULES_FILE	__XKBDEFRULES__
#endif
#ifndef XKB_DFLT_KB_LAYOUT
#define	XKB_DFLT_KB_LAYOUT	"us"
#endif
#ifndef XKB_DFLT_KB_MODEL
#define	XKB_DFLT_KB_MODEL	"pc101"
#endif
#ifndef XKB_DFLT_KB_VARIANT
#define	XKB_DFLT_KB_VARIANT	NULL
#endif
#ifndef XKB_DFLT_KB_OPTIONS
#define	XKB_DFLT_KB_OPTIONS	NULL
#endif

#endif

#define XGLX_DEFAULT_SCREEN_WIDTH  800
#define XGLX_DEFAULT_SCREEN_HEIGHT 600

#define MAX_BUTTONS 64

typedef struct _xglxScreen {
    Window	       win, root;
    Colormap	       colormap;
    Bool	       fullscreen;
    CloseScreenProcPtr CloseScreen;
} xglxScreenRec, *xglxScreenPtr;

int xglxScreenGeneration = -1;
int xglxScreenPrivateIndex;

#define XGLX_GET_SCREEN_PRIV(pScreen)				         \
    ((xglxScreenPtr) (pScreen)->devPrivates[xglxScreenPrivateIndex].ptr)

#define XGLX_SET_SCREEN_PRIV(pScreen, v)			       \
    ((pScreen)->devPrivates[xglxScreenPrivateIndex].ptr = (pointer) v)

#define XGLX_SCREEN_PRIV(pScreen)			       \
    xglxScreenPtr pScreenPriv = XGLX_GET_SCREEN_PRIV (pScreen)

typedef struct _xglxCursor {
    Cursor cursor;
} xglxCursorRec, *xglxCursorPtr;

#define XGLX_GET_CURSOR_PRIV(pCursor, pScreen)		   \
    ((xglxCursorPtr) (pCursor)->devPriv[(pScreen)->myNum])

#define XGLX_SET_CURSOR_PRIV(pCursor, pScreen, v)	 \
    ((pCursor)->devPriv[(pScreen)->myNum] = (pointer) v)

#define XGLX_CURSOR_PRIV(pCursor, pScreen)			        \
    xglxCursorPtr pCursorPriv = XGLX_GET_CURSOR_PRIV (pCursor, pScreen)

static char	 *xDisplayName = 0;
static Display	 *xdisplay     = 0;
static int	 xscreen;
static CARD32	 lastEventTime = 0;
static Bool	 softCursor    = FALSE;
static Bool	 fullscreen    = TRUE;
static Bool	 xDpms         = FALSE;
static int	 displayOffset = 0;
static int	 numScreen     = 1;
static int	 primaryScreen = 0;

static Bool randrExtension = FALSE;
static int  randrEvent, randrError;

static glitz_drawable_format_t *xglxScreenFormat = 0;

static RegionRec screenRegion;
static BoxPtr	 screenRect  = NULL;
static int	 nScreenRect = 0;

static Window currentEventWindow = None;

static Bool
xglxAllocatePrivates (ScreenPtr pScreen)
{
    xglxScreenPtr pScreenPriv;

    if (xglxScreenGeneration != serverGeneration)
    {
	xglxScreenPrivateIndex = AllocateScreenPrivateIndex ();
	if (xglxScreenPrivateIndex < 0)
	    return FALSE;

	xglxScreenGeneration = serverGeneration;
    }

    pScreenPriv = xalloc (sizeof (xglxScreenRec));
    if (!pScreenPriv)
	return FALSE;

    XGLX_SET_SCREEN_PRIV (pScreen, pScreenPriv);

    return TRUE;
}

#ifdef RANDR

#define DEFAULT_REFRESH_RATE 50

static Bool
xglxRandRGetInfo (ScreenPtr pScreen,
		  Rotation  *rotations)
{
    RRScreenSizePtr pSize;

    *rotations = RR_Rotate_0;

    if (randrExtension)
    {
	XRRScreenConfiguration *xconfig;
	XRRScreenSize	       *sizes;
	int		       nSizes, currentSize = 0;
	short		       *rates, currentRate;
	int		       nRates, i, j;

	XGLX_SCREEN_PRIV (pScreen);

	xconfig	    = XRRGetScreenInfo (xdisplay, pScreenPriv->root);
	sizes	    = XRRConfigSizes (xconfig, &nSizes);
	currentRate = XRRConfigCurrentRate (xconfig);

	if (pScreenPriv->fullscreen && nScreenRect == 0)
	{
	    Rotation rotation;

	    currentSize = XRRConfigCurrentConfiguration (xconfig, &rotation);

	    for (i = 0; i < nSizes; i++)
	    {
		pSize = RRRegisterSize (pScreen,
					sizes[i].width,
					sizes[i].height,
					sizes[i].mwidth,
					sizes[i].mheight);

		rates = XRRConfigRates (xconfig, i, &nRates);

		for (j = 0; j < nRates; j++)
		{
		    RRRegisterRate (pScreen, pSize, rates[j]);

		    if (i == currentSize && rates[j] == currentRate)
			RRSetCurrentConfig (pScreen, RR_Rotate_0, currentRate,
					    pSize);
		}
	    }
	}
	else
	{
	    pSize = RRRegisterSize (pScreen,
				    pScreen->width,
				    pScreen->height,
				    pScreen->mmWidth,
				    pScreen->mmHeight);

	    for (i = 0; i < nSizes; i++)
	    {
		rates = XRRConfigRates (xconfig, i, &nRates);

		for (j = 0; j < nRates; j++)
		{
		    RRRegisterRate (pScreen, pSize, rates[j]);

		    if (rates[j] == currentRate)
			RRSetCurrentConfig (pScreen, RR_Rotate_0, currentRate,
					    pSize);
		}
	    }
	}

	XRRFreeScreenConfigInfo (xconfig);
    }
    else
    {
	pSize = RRRegisterSize (pScreen,
				pScreen->width,
				pScreen->height,
				pScreen->mmWidth,
				pScreen->mmHeight);

	RRRegisterRate (pScreen, pSize, DEFAULT_REFRESH_RATE);
	RRSetCurrentConfig (pScreen, RR_Rotate_0, DEFAULT_REFRESH_RATE, pSize);
    }

    return TRUE;
}

static Bool
xglxRandRSetConfig (ScreenPtr	    pScreen,
		    Rotation	    rotations,
		    int		    rate,
		    RRScreenSizePtr pSize)
{
    if (randrExtension)
    {
	XRRScreenConfiguration *xconfig;
	XRRScreenSize	       *sizes;
	int		       nSizes, currentSize;
	int		       i, size = -1;
	int		       status = RRSetConfigFailed;
	Rotation	       rotation;

	XGLX_SCREEN_PRIV (pScreen);

	xconfig	    = XRRGetScreenInfo (xdisplay, pScreenPriv->root);
	sizes	    = XRRConfigSizes (xconfig, &nSizes);
	currentSize = XRRConfigCurrentConfiguration (xconfig, &rotation);

	for (i = 0; i < nSizes; i++)
	{
	    if (pScreenPriv->fullscreen && nScreenRect == 0)
	    {
		if (sizes[i].width   == pSize->width   &&
		    sizes[i].height  == pSize->height  &&
		    sizes[i].mwidth  == pSize->mmWidth &&
		    sizes[i].mheight == pSize->mmHeight)
		{
		    size = i;
		    break;
		}
	    }
	    else
	    {
		short *rates;
		int   nRates, j;

		rates = XRRConfigRates (xconfig, i, &nRates);

		for (j = 0; j < nRates; j++)
		{
		    if (rates[j] == rate)
		    {
			size = i;
			if (i >= currentSize)
			    break;
		    }
		}
	    }
	}

	if (size >= 0)
	    status = XRRSetScreenConfigAndRate (xdisplay,
						xconfig,
						pScreenPriv->root,
						size,
						RR_Rotate_0,
						rate,
						CurrentTime);

	XRRFreeScreenConfigInfo (xconfig);

	if (status == RRSetConfigSuccess)
	{
	    PixmapPtr pPixmap;

	    pPixmap = (*pScreen->GetScreenPixmap) (pScreen);

	    if (pScreenPriv->fullscreen)
	    {
		XGL_PIXMAP_PRIV (pPixmap);

		xglSetRootClip (pScreen, FALSE);

		XResizeWindow (xdisplay, pScreenPriv->win,
			       pSize->width, pSize->height);

		glitz_drawable_update_size (pPixmapPriv->drawable,
					    pSize->width, pSize->height);

		pScreen->width    = pSize->width;
		pScreen->height   = pSize->height;
		pScreen->mmWidth  = pSize->mmWidth;
		pScreen->mmHeight = pSize->mmHeight;

		(*pScreen->ModifyPixmapHeader) (pPixmap,
						pScreen->width,
						pScreen->height,
						pPixmap->drawable.depth,
						pPixmap->drawable.bitsPerPixel,
						0, 0);

		xglSetRootClip (pScreen, TRUE);
	    }

	    return TRUE;
	}
    }

    return FALSE;
}

static Bool
xglxRandRInit (ScreenPtr pScreen)
{
    rrScrPrivPtr pScrPriv;

    if (!RRScreenInit (pScreen))
	return FALSE;

    pScrPriv = rrGetScrPriv (pScreen);
    pScrPriv->rrGetInfo   = xglxRandRGetInfo;
    pScrPriv->rrSetConfig = xglxRandRSetConfig;

    return TRUE;
}

#endif

static Bool
xglxExposurePredicate (Display *xdisplay,
		       XEvent  *xevent,
		       char    *args)
{
    return (xevent->type == Expose);
}

static Bool
xglxNotExposurePredicate (Display *xdisplay,
			  XEvent  *xevent,
			  char	  *args)
{
    return (xevent->type != Expose);
}

static int
xglxWindowExposures (WindowPtr pWin,
		     pointer   pReg)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RegionRec ClipList;

    if (HasBorder (pWin))
    {
	REGION_INIT (pScreen, &ClipList, NullBox, 0);
	REGION_SUBTRACT (pScreen, &ClipList, &pWin->borderClip,
			 &pWin->winSize);
	REGION_INTERSECT (pScreen, &ClipList, &ClipList, (RegionPtr) pReg);
	(*pScreen->PaintWindowBorder) (pWin, &ClipList, PW_BORDER);
	REGION_UNINIT (pScreen, &ClipList);
    }

    REGION_INIT (pScreen, &ClipList, NullBox, 0);
    REGION_INTERSECT (pScreen, &ClipList, &pWin->clipList, (RegionPtr) pReg);
    (*pScreen->WindowExposures) (pWin, &ClipList, NullRegion);
    REGION_UNINIT (pScreen, &ClipList);

    return WT_WALKCHILDREN;
}

static void
xglxEnqueueEvents (void)
{
    XEvent X;
    xEvent x;

    while (XCheckIfEvent (xdisplay, &X, xglxNotExposurePredicate, NULL))
    {
	switch (X.type) {
	case KeyPress:
	    x.u.u.type = KeyPress;
	    x.u.u.detail = X.xkey.keycode;
	    x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis ();
	    mieqEnqueue (&x);
	    break;
	case KeyRelease:
	    x.u.u.type = KeyRelease;
	    x.u.u.detail = X.xkey.keycode;
	    x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis ();
	    mieqEnqueue (&x);
	    break;
	case ButtonPress:
	    x.u.u.type = ButtonPress;
	    x.u.u.detail = X.xbutton.button;
	    x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis ();
	    mieqEnqueue (&x);
	    break;
	case ButtonRelease:
	    x.u.u.type = ButtonRelease;
	    x.u.u.detail = X.xbutton.button;
	    x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis ();
	    mieqEnqueue (&x);
	    break;
	case MotionNotify:
	    x.u.u.type = MotionNotify;
	    x.u.u.detail = 0;
	    x.u.keyButtonPointer.rootX = X.xmotion.x;
	    x.u.keyButtonPointer.rootY = X.xmotion.y;
	    x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis ();
	    miPointerAbsoluteCursor (X.xmotion.x, X.xmotion.y, lastEventTime);
	    mieqEnqueue (&x);
	    break;
	case EnterNotify:
	    if (!nScreenRect && X.xcrossing.detail != NotifyInferior)
	    {
		ScreenPtr pScreen = 0;
		int	  i;

		for (i = 0; i < screenInfo.numScreens; i++)
		{
		    XGLX_SCREEN_PRIV (screenInfo.screens[i]);

		    if (pScreenPriv->win == X.xcrossing.window)
		    {
			pScreen	= screenInfo.screens[i];
			break;
		    }
		}

		if (pScreen)
		{
		    NewCurrentScreen (pScreen, X.xcrossing.x, X.xcrossing.y);

		    x.u.u.type = MotionNotify;
		    x.u.u.detail = 0;
		    x.u.keyButtonPointer.rootX = X.xcrossing.x;
		    x.u.keyButtonPointer.rootY = X.xcrossing.y;
		    x.u.keyButtonPointer.time = lastEventTime =
			GetTimeInMillis ();
		    mieqEnqueue (&x);
		}
	    }
	    break;
	default:
	    break;
	}
    }
}

#ifdef PANORAMIX

static int xglxXineramaGeneration = -1;

static void
xglxXineramaResetProc (ExtensionEntry *extEntry)
{
}

static int
xglxProcXineramaQueryVersion (ClientPtr client)
{
    xPanoramiXQueryVersionReply	rep;
    register int		n;
    int				majorVersion, minorVersion;

    REQUEST_SIZE_MATCH (xPanoramiXQueryVersionReq);

    XineramaQueryVersion (xdisplay, &majorVersion, &minorVersion);

    rep.type	       = X_Reply;
    rep.length	       = 0;
    rep.sequenceNumber = client->sequence;
    rep.majorVersion   = majorVersion;
    rep.minorVersion   = minorVersion;

    if (client->swapped)
    {
	swaps (&rep.sequenceNumber, n);
	swapl (&rep.length, n);
	swaps (&rep.majorVersion, n);
	swaps (&rep.minorVersion, n);
    }

    WriteToClient (client, sizeof (xPanoramiXQueryVersionReply), (char *) &rep);

    return client->noClientException;
}

static int
xglxProcXineramaIsActive (ClientPtr client)
{
    xXineramaIsActiveReply rep;

    REQUEST_SIZE_MATCH (xXineramaIsActiveReq);

    rep.type	       = X_Reply;
    rep.length	       = 0;
    rep.sequenceNumber = client->sequence;
    rep.state	       = XineramaIsActive (xdisplay);

    if (client->swapped)
    {
	register int n;

	swaps (&rep.sequenceNumber, n);
	swapl (&rep.length, n);
	swapl (&rep.state, n);
    }

    WriteToClient (client, sizeof (xXineramaIsActiveReply), (char *) &rep);

    return client->noClientException;
}

static int
xglxProcXineramaQueryScreens (ClientPtr client)
{
    xXineramaQueryScreensReply rep;
    xXineramaScreenInfo	       scratch;
    XineramaScreenInfo	       *info;
    int			       n;

    REQUEST_SIZE_MATCH (xXineramaQueryScreensReq);

    info = XineramaQueryScreens (xdisplay, &n);

    rep.type	       = X_Reply;
    rep.sequenceNumber = client->sequence;

    if (info)
    {
	rep.number = n;
	rep.length = n * sz_XineramaScreenInfo >> 2;
    }
    else
    {
	rep.number = 0;
	rep.length = 0;
    }

    if (client->swapped)
    {
	register int n;

	swaps (&rep.sequenceNumber, n);
	swapl (&rep.length, n);
	swapl (&rep.number, n);
    }

    WriteToClient (client, sizeof (xXineramaQueryScreensReply), (char *) &rep);

    if (info)
    {
	xXineramaScreenInfo scratch;
	int		    i;

	for (i = 0; i < rep.number; i++)
	{
	    scratch.x_org  = info[i].x_org;
	    scratch.y_org  = info[i].y_org;
	    scratch.width  = info[i].width;
	    scratch.height = info[i].height;

	    if (client->swapped)
	    {
		register int n;

		swaps (&scratch.x_org, n);
		swaps (&scratch.y_org, n);
		swaps (&scratch.width, n);
		swaps (&scratch.height, n);
	    }

	    WriteToClient (client, sz_XineramaScreenInfo, (char *) &scratch);
	}

	XFree (info);
    }

    return client->noClientException;
}

static int
xglxProcXineramaDispatch (ClientPtr client)
{
    REQUEST (xReq);

    switch (stuff->data) {
    case X_PanoramiXQueryVersion:
	return xglxProcXineramaQueryVersion (client);
    case X_XineramaIsActive:
	return xglxProcXineramaIsActive (client);
    case X_XineramaQueryScreens:
	return xglxProcXineramaQueryScreens (client);
    }

    return BadRequest;
}

static Bool
xglxXineramaInit (void)
{
    ExtensionEntry *extEntry;

    if (xglxXineramaGeneration != serverGeneration)
    {
	extEntry = AddExtension (PANORAMIX_PROTOCOL_NAME, 0,0,
				 xglxProcXineramaDispatch,
				 xglxProcXineramaDispatch,
				 xglxXineramaResetProc,
				 StandardMinorOpcode);
	if (!extEntry)
	    return FALSE;

	xglxXineramaGeneration = serverGeneration;
    }

    return TRUE;
}

#endif

static void
xglxConstrainCursor (ScreenPtr pScreen,
		     BoxPtr    pBox)
{
}

static void
xglxCursorLimits (ScreenPtr pScreen,
		  CursorPtr pCursor,
		  BoxPtr    pHotBox,
		  BoxPtr    pTopLeftBox)
{
    *pTopLeftBox = *pHotBox;
}

static Bool
xglxDisplayCursor (ScreenPtr pScreen,
		   CursorPtr pCursor)
{
    XGLX_SCREEN_PRIV (pScreen);
    XGLX_CURSOR_PRIV (pCursor, pScreen);

    XDefineCursor (xdisplay, pScreenPriv->win, pCursorPriv->cursor);

    return TRUE;
}

#ifdef ARGB_CURSOR

static Bool
xglxARGBCursorSupport (void);

static Cursor
xglxCreateARGBCursor (ScreenPtr pScreen,
		      CursorPtr pCursor);

#endif

static Bool
xglxRealizeCursor (ScreenPtr pScreen,
		   CursorPtr pCursor)
{
    xglxCursorPtr pCursorPriv;
    XImage	  *ximage;
    Pixmap	  source, mask;
    XColor	  fgColor, bgColor;
    XlibGC	  xgc;
    unsigned long valuemask;
    XGCValues	  values;

    XGLX_SCREEN_PRIV (pScreen);

    valuemask = GCForeground | GCBackground;

    values.foreground = 1L;
    values.background = 0L;

    pCursorPriv = xalloc (sizeof (xglxCursorRec));
    if (!pCursorPriv)
	return FALSE;

    XGLX_SET_CURSOR_PRIV (pCursor, pScreen, pCursorPriv);

#ifdef ARGB_CURSOR
    if (pCursor->bits->argb)
    {
	pCursorPriv->cursor = xglxCreateARGBCursor (pScreen, pCursor);
	if (pCursorPriv->cursor)
	    return TRUE;
    }
#endif

    source = XCreatePixmap (xdisplay,
			    pScreenPriv->win,
			    pCursor->bits->width,
			    pCursor->bits->height,
			    1);

    mask = XCreatePixmap (xdisplay,
			  pScreenPriv->win,
			  pCursor->bits->width,
			  pCursor->bits->height,
			  1);

    xgc = XCreateGC (xdisplay, source, valuemask, &values);

    ximage = XCreateImage (xdisplay,
			   DefaultVisual (xdisplay, xscreen),
			   1, XYBitmap, 0,
			   (char *) pCursor->bits->source,
			   pCursor->bits->width,
			   pCursor->bits->height,
			   BitmapPad (xdisplay), 0);

    XPutImage (xdisplay, source, xgc, ximage,
	       0, 0, 0, 0, pCursor->bits->width, pCursor->bits->height);

    XFree (ximage);

    ximage = XCreateImage (xdisplay,
			   DefaultVisual (xdisplay, xscreen),
			   1, XYBitmap, 0,
			   (char *) pCursor->bits->mask,
			   pCursor->bits->width,
			   pCursor->bits->height,
			   BitmapPad (xdisplay), 0);

    XPutImage (xdisplay, mask, xgc, ximage,
	       0, 0, 0, 0, pCursor->bits->width, pCursor->bits->height);

    XFree (ximage);
    XFreeGC (xdisplay, xgc);

    fgColor.red   = pCursor->foreRed;
    fgColor.green = pCursor->foreGreen;
    fgColor.blue  = pCursor->foreBlue;

    bgColor.red   = pCursor->backRed;
    bgColor.green = pCursor->backGreen;
    bgColor.blue  = pCursor->backBlue;

    pCursorPriv->cursor =
	XCreatePixmapCursor (xdisplay, source, mask, &fgColor, &bgColor,
			     pCursor->bits->xhot, pCursor->bits->yhot);

    XFreePixmap (xdisplay, mask);
    XFreePixmap (xdisplay, source);

    return TRUE;
}

static Bool
xglxUnrealizeCursor (ScreenPtr pScreen,
		     CursorPtr pCursor)
{
    XGLX_CURSOR_PRIV (pCursor, pScreen);

    XFreeCursor (xdisplay, pCursorPriv->cursor);
    xfree (pCursorPriv);

    return TRUE;
}

static void
xglxRecolorCursor (ScreenPtr pScreen,
		   CursorPtr pCursor,
		   Bool	     displayed)
{
    XColor fgColor, bgColor;

    XGLX_CURSOR_PRIV (pCursor, pScreen);

    fgColor.red   = pCursor->foreRed;
    fgColor.green = pCursor->foreGreen;
    fgColor.blue  = pCursor->foreBlue;

    bgColor.red   = pCursor->backRed;
    bgColor.green = pCursor->backGreen;
    bgColor.blue  = pCursor->backBlue;

    XRecolorCursor (xdisplay, pCursorPriv->cursor, &fgColor, &bgColor);
}

static Bool
xglxSetCursorPosition (ScreenPtr pScreen,
		       int	 x,
		       int	 y,
		       Bool	 generateEvent)
{
    XGLX_SCREEN_PRIV (pScreen);

    if (currentEventWindow != pScreenPriv->win)
    {
	currentEventWindow = pScreenPriv->win;

	if (nScreenRect)
	    XGrabPointer (xdisplay,
			  currentEventWindow,
			  TRUE,
			  ButtonPressMask   |
			  ButtonReleaseMask |
			  PointerMotionMask,
			  GrabModeAsync,
			  GrabModeAsync,
			  currentEventWindow,
			  None,
			  CurrentTime);
    }

    XWarpPointer (xdisplay, currentEventWindow, pScreenPriv->win,
		  0, 0, 0, 0, x, y);

    if (generateEvent)
    {
	XSync (xdisplay, FALSE);
	xglxEnqueueEvents ();
    }

    return TRUE;
}

static Bool
xglxCloseScreen (int	   index,
		 ScreenPtr pScreen)
{
    glitz_drawable_t *drawable;

    XGLX_SCREEN_PRIV (pScreen);

    drawable = XGL_GET_SCREEN_PRIV (pScreen)->drawable;
    if (drawable)
	glitz_drawable_destroy (drawable);

    xglClearVisualTypes ();

    if (pScreenPriv->win)
	XDestroyWindow (xdisplay, pScreenPriv->win);

    if (pScreenPriv->colormap)
	XFreeColormap (xdisplay, pScreenPriv->colormap);

    XGL_SCREEN_UNWRAP (CloseScreen);
    xfree (pScreenPriv);

    return (*pScreen->CloseScreen) (index, pScreen);
}

static Bool
xglxCursorOffScreen (ScreenPtr *ppScreen, int *x, int *y)
{
    return FALSE;
}

static void
xglxCrossScreen (ScreenPtr pScreen, Bool entering)
{
}

static void
xglxWarpCursor (ScreenPtr pScreen, int x, int y)
{
    miPointerWarpCursor (pScreen, x, y);
}

miPointerScreenFuncRec xglxPointerScreenFuncs = {
    xglxCursorOffScreen,
    xglxCrossScreen,
    xglxWarpCursor
};

static Bool
xglxScreenInit (int	  index,
		ScreenPtr pScreen,
		int	  argc,
		char	  **argv)
{
    XSetWindowAttributes    xswa;
    XWMHints		    *wmHints;
    XSizeHints		    *normalHints;
    XClassHint		    *classHint;
    xglxScreenPtr	    pScreenPriv;
    XVisualInfo		    *vinfo;
    XEvent		    xevent;
    glitz_drawable_format_t *format;
    glitz_drawable_t	    *drawable;
    int			    x = 0, y = 0;

    format = xglxScreenFormat;

    if (!xglxAllocatePrivates (pScreen))
	return FALSE;

    pScreenPriv = XGLX_GET_SCREEN_PRIV (pScreen);

    pScreenPriv->root	    = RootWindow (xdisplay, xscreen);
    pScreenPriv->fullscreen = fullscreen;

    vinfo = glitz_glx_get_visual_info_from_format (xdisplay, xscreen, format);
    if (!vinfo)
    {
	ErrorF ("[%d] no visual info from format\n", index);
	return FALSE;
    }

    pScreenPriv->colormap =
	XCreateColormap (xdisplay, pScreenPriv->root, vinfo->visual,
			 AllocNone);

    if (XRRQueryExtension (xdisplay, &randrEvent, &randrError))
	randrExtension = TRUE;

    if (fullscreen)
    {
	xglScreenInfo.width    = DisplayWidth (xdisplay, xscreen);
	xglScreenInfo.height   = DisplayHeight (xdisplay, xscreen);
	xglScreenInfo.widthMm  = DisplayWidthMM (xdisplay, xscreen);
	xglScreenInfo.heightMm = DisplayHeightMM (xdisplay, xscreen);

	if (randrExtension)
	{
	    XRRScreenConfiguration *xconfig;
	    Rotation		   rotation;
	    XRRScreenSize	   *sizes;
	    int			   nSizes, currentSize;

	    xconfig	= XRRGetScreenInfo (xdisplay, pScreenPriv->root);
	    currentSize = XRRConfigCurrentConfiguration (xconfig, &rotation);
	    sizes	= XRRConfigSizes (xconfig, &nSizes);

	    xglScreenInfo.width    = sizes[currentSize].width;
	    xglScreenInfo.height   = sizes[currentSize].height;
	    xglScreenInfo.widthMm  = sizes[currentSize].mwidth;
	    xglScreenInfo.heightMm = sizes[currentSize].mheight;

	    XRRFreeScreenConfigInfo (xconfig);
	}

	if (nScreenRect)
	{
	    int w, h;

	    x = screenRect[index].x1;
	    y = screenRect[index].y1;
	    w = screenRect[index].x2 - x;
	    h = screenRect[index].y2 - y;

	    xglScreenInfo.widthMm = (xglScreenInfo.widthMm * w) /
		xglScreenInfo.width;
	    xglScreenInfo.width   = w;

	    xglScreenInfo.heightMm = (xglScreenInfo.heightMm * h) /
		xglScreenInfo.height;
	    xglScreenInfo.height   = h;
	}
    }
    else if (xglScreenInfo.width == 0 || xglScreenInfo.height == 0)
    {
	xglScreenInfo.width  = XGLX_DEFAULT_SCREEN_WIDTH;
	xglScreenInfo.height = XGLX_DEFAULT_SCREEN_HEIGHT;
    }

    xswa.colormap = pScreenPriv->colormap;

    pScreenPriv->win =
	XCreateWindow (xdisplay, pScreenPriv->root, x, y,
		       xglScreenInfo.width, xglScreenInfo.height, 0,
		       vinfo->depth, InputOutput, vinfo->visual,
		       CWColormap, &xswa);

    XFree (vinfo);

    normalHints = XAllocSizeHints ();
    normalHints->flags      = PMinSize | PMaxSize | PSize;
    normalHints->min_width  = xglScreenInfo.width;
    normalHints->min_height = xglScreenInfo.height;
    normalHints->max_width  = xglScreenInfo.width;
    normalHints->max_height = xglScreenInfo.height;

    if (fullscreen)
    {
	normalHints->x = x;
	normalHints->y = y;
	normalHints->flags |= PPosition;
    }
    else
    {
	currentEventWindow = pScreenPriv->win;
    }

    classHint = XAllocClassHint ();
    classHint->res_name = "xglx";
    classHint->res_class = "Xglx";

    wmHints = XAllocWMHints ();
    wmHints->flags = InputHint;
    wmHints->input = TRUE;

    Xutf8SetWMProperties (xdisplay, pScreenPriv->win, "Xglx", "Xglx", 0, 0,
			  normalHints, wmHints, classHint);

    XFree (wmHints);
    XFree (classHint);
    XFree (normalHints);

    drawable = glitz_glx_create_drawable_for_window (xdisplay, xscreen,
						     format, pScreenPriv->win,
						     xglScreenInfo.width,
						     xglScreenInfo.height);
    if (!drawable)
    {
	ErrorF ("[%d] couldn't create glitz drawable for window\n", index);
	return FALSE;
    }

    XSelectInput (xdisplay, pScreenPriv->win,
		  ButtonPressMask | ButtonReleaseMask |
		  KeyPressMask | KeyReleaseMask | EnterWindowMask |
		  PointerMotionMask | ExposureMask);

    XMapWindow (xdisplay, pScreenPriv->win);

    if (fullscreen)
    {
	XClientMessageEvent xev;

	memset (&xev, 0, sizeof (xev));

	xev.type = ClientMessage;
	xev.message_type = XInternAtom (xdisplay, "_NET_WM_STATE", FALSE);
	xev.display = xdisplay;
	xev.window = pScreenPriv->win;
	xev.format = 32;
	xev.data.l[0] = 1;
	xev.data.l[1] =
	    XInternAtom (xdisplay, "_NET_WM_STATE_FULLSCREEN", FALSE);

	XSendEvent (xdisplay, pScreenPriv->root, FALSE,
		    SubstructureRedirectMask, (XEvent *) &xev);
    }

    xglScreenInfo.drawable = drawable;

    if (!xglScreenInit (pScreen))
	return FALSE;

#ifdef GLXEXT
    if (!xglInitVisualConfigs (pScreen))
	return FALSE;
#endif

    XGL_SCREEN_WRAP (CloseScreen, xglxCloseScreen);

#ifdef ARGB_CURSOR
    if (!xglxARGBCursorSupport ())
	softCursor = TRUE;
#endif

    if (softCursor)
    {
	static char data = 0;
	XColor	    black, dummy;
	Pixmap	    bitmap;
	Cursor	    cursor;
	XID	    installedCmaps;

	if (!XAllocNamedColor (xdisplay, pScreenPriv->colormap,
			       "black", &black, &dummy))
	    return FALSE;

	bitmap = XCreateBitmapFromData (xdisplay, pScreenPriv->win, &data,
					1, 1);
	if (!bitmap)
	    return FALSE;

	cursor = XCreatePixmapCursor (xdisplay, bitmap, bitmap, &black, &black,
				      0, 0);
	if (!cursor)
	    return FALSE;

	XDefineCursor (xdisplay, pScreenPriv->win, cursor);

	XFreeCursor (xdisplay, cursor);
	XFreePixmap (xdisplay, bitmap);
	XFreeColors (xdisplay, pScreenPriv->colormap, &black.pixel, 1, 0);

	miDCInitialize (pScreen, &xglxPointerScreenFuncs);

	/* XXX: Assumes only one installed colormap. */
	if ((*pScreen->ListInstalledColormaps) (pScreen, &installedCmaps))
	{
	    ColormapPtr installedCmap;

	    installedCmap = LookupIDByType (installedCmaps, RT_COLORMAP);
	    if (installedCmap)
		(*pScreen->InstallColormap) (installedCmap);
	}
    }
    else
    {
	pScreen->ConstrainCursor   = xglxConstrainCursor;
	pScreen->CursorLimits      = xglxCursorLimits;
	pScreen->DisplayCursor     = xglxDisplayCursor;
	pScreen->RealizeCursor     = xglxRealizeCursor;
	pScreen->UnrealizeCursor   = xglxUnrealizeCursor;
	pScreen->RecolorCursor     = xglxRecolorCursor;
	pScreen->SetCursorPosition = xglxSetCursorPosition;
    }

    if (!xglFinishScreenInit (pScreen))
	return FALSE;

#ifdef RANDR
    if (!xglxRandRInit (pScreen))
	return FALSE;
#endif

    while (XNextEvent (xdisplay, &xevent))
	if (xevent.type == Expose)
	    break;

    return TRUE;
}

void
xglxInitOutput (ScreenInfo *pScreenInfo,
		int	   argc,
		char	   **argv)
{
    glitz_drawable_format_t *format, templ;
    int			    i;
    unsigned long	    mask;
    unsigned long	    extraMask[] = {
	GLITZ_FORMAT_DOUBLEBUFFER_MASK | GLITZ_FORMAT_ALPHA_SIZE_MASK,
	GLITZ_FORMAT_DOUBLEBUFFER_MASK,
	GLITZ_FORMAT_ALPHA_SIZE_MASK,
	0
    };

    xglClearVisualTypes ();

    xglSetPixmapFormats (pScreenInfo);

    templ.samples          = 1;
    templ.doublebuffer     = 1;
    templ.color.fourcc     = GLITZ_FOURCC_RGB;
    templ.color.alpha_size = 8;

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK;

    for (i = 0; i < sizeof (extraMask) / sizeof (extraMask[0]); i++)
    {
	format = glitz_glx_find_window_format (xdisplay, xscreen,
					       mask | extraMask[i],
					       &templ, 0);
	if (format)
	    break;
    }

    if (!format)
	FatalError ("no visual format found\n");

    xglScreenInfo.depth =
	format->color.red_size   +
	format->color.green_size +
	format->color.blue_size;

    xglSetVisualTypes (xglScreenInfo.depth,
		       (1 << TrueColor),
		       format->color.red_size,
		       format->color.green_size,
		       format->color.blue_size);

    xglxScreenFormat = format;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
	xglxXineramaInit ();
#endif

    for (i = 0; i < numScreen; i++)
	AddScreen (xglxScreenInit, argc, argv);
}

static void
xglxBlockHandler (pointer   blockData,
		  OSTimePtr pTimeout,
		  pointer   pReadMask)
{
    XEvent    X;
    RegionRec region;
    BoxRec    box;
    ScreenPtr pScreen;
    int	      i;

    for (i = 0; i < screenInfo.numScreens; i++)
    {
	pScreen = screenInfo.screens[i];

	XGL_SCREEN_PRIV (pScreen);

	while (XCheckIfEvent (xdisplay, &X, xglxExposurePredicate, NULL))
	{
	    box.x1 = X.xexpose.x;
	    box.y1 = X.xexpose.y;
	    box.x2 = box.x1 + X.xexpose.width;
	    box.y2 = box.y1 + X.xexpose.height;

	    REGION_INIT (pScreen, &region, &box, 1);

	    WalkTree (pScreen, xglxWindowExposures, &region);

	    REGION_UNINIT (pScreen, &region);
	}

	if (!xglSyncSurface (&pScreenPriv->pScreenPixmap->drawable))
	    FatalError (XGL_SW_FAILURE_STRING);

	glitz_surface_flush (pScreenPriv->surface);
	glitz_drawable_flush (pScreenPriv->drawable);
    }

    XFlush (xdisplay);
}

static void
xglxWakeupHandler (pointer blockData,
		   int     result,
		   pointer pReadMask)
{
    xglxEnqueueEvents ();
}

static void
xglxBell (int	       volume,
	  DeviceIntPtr pDev,
	  pointer      ctrl,
	  int	       cls)
{
    XBell (xdisplay, volume);
}

static void
xglxKbdCtrl (DeviceIntPtr pDev,
	     KeybdCtrl    *ctrl)
{
    unsigned long    valueMask;
    XKeyboardControl values;
    int		     i;

    valueMask = KBKeyClickPercent | KBBellPercent | KBBellPitch |
	KBBellDuration | KBAutoRepeatMode;

    values.key_click_percent = ctrl->click;
    values.bell_percent	     = ctrl->bell;
    values.bell_pitch	     = ctrl->bell_pitch;
    values.bell_duration     = ctrl->bell_duration;
    values.auto_repeat_mode  = (ctrl->autoRepeat) ? AutoRepeatModeOn :
	AutoRepeatModeOff;

    XChangeKeyboardControl (xdisplay, valueMask, &values);

    valueMask = KBLed | KBLedMode;

    for (i = 0; i < 5; i++)
    {
	values.led = i + 1;
	values.led_mode = (ctrl->leds & (1 << i)) ? LedModeOn : LedModeOff;

	XChangeKeyboardControl (xdisplay, valueMask, &values);
    }
}

static int
xglxKeybdProc (DeviceIntPtr pDevice,
	       int	    onoff)
{
    Bool      ret = FALSE;
    DevicePtr pDev = (DevicePtr) pDevice;

    if (!pDev)
	return BadImplementation;

    switch (onoff) {
    case DEVICE_INIT: {
      XModifierKeymap *xmodMap;
      KeySym	      *xkeyMap;
      int	      minKeyCode, maxKeyCode, mapWidth, i, j;
      KeySymsRec      xglxKeySyms;
      CARD8	      xglxModMap[256];
      XKeyboardState  values;

#ifdef _XSERVER64
      KeySym64	      *xkeyMap64;
      int	      len;
#endif

#ifdef XKB
      Bool	      xkbExtension = FALSE;
      int	      xkbOp, xkbEvent, xkbError, xkbMajor, xkbMinor;
#endif

      if (pDev != LookupKeyboardDevice ())
	  return !Success;

      xmodMap = XGetModifierMapping (xdisplay);

      XDisplayKeycodes (xdisplay, &minKeyCode, &maxKeyCode);

#ifdef _XSERVER64
      xkeyMap64 = XGetKeyboardMapping (xdisplay,
				       minKeyCode,
				       maxKeyCode - minKeyCode + 1,
				       &mapWidth);

      len = (maxKeyCode - minKeyCode + 1) * mapWidth;
      xkeyMap = (KeySym *) xalloc (len * sizeof (KeySym));
      for (i = 0; i < len; ++i)
	  xkeyMap[i] = xkeyMap64[i];

      XFree (xkeyMap64);
#else
      xkeyMap = XGetKeyboardMapping (xdisplay,
				     minKeyCode,
				     maxKeyCode - minKeyCode + 1,
				     &mapWidth);
#endif

      memset (xglxModMap, 0, 256);

      for (j = 0; j < 8; j++)
      {
	  for (i = 0; i < xmodMap->max_keypermod; i++)
	  {
	      CARD8 keyCode;

	      keyCode = xmodMap->modifiermap[j * xmodMap->max_keypermod + i];
	      if (keyCode)
		  xglxModMap[keyCode] |= 1 << j;
	  }
      }

      XFreeModifiermap (xmodMap);

      xglxKeySyms.minKeyCode = minKeyCode;
      xglxKeySyms.maxKeyCode = maxKeyCode;
      xglxKeySyms.mapWidth   = mapWidth;
      xglxKeySyms.map	     = xkeyMap;

#ifdef XKB
      if (!noXkbExtension)
	  xkbExtension = XkbQueryExtension (xdisplay,
					    &xkbOp, &xkbEvent, &xkbError,
					    &xkbMajor, &xkbMinor);

      if (xkbExtension)
      {
	  XkbRF_VarDefsRec vd;
	  XkbDescPtr	   desc;
	  char		   *rules, *model, *layout, *variants, *options;
	  char		   *tmp = NULL;

	  rules    = XKB_DFLT_RULES_FILE;
	  model    = XKB_DFLT_KB_MODEL;
	  layout   = XKB_DFLT_KB_LAYOUT;
	  variants = XKB_DFLT_KB_VARIANT;
	  options  = XKB_DFLT_KB_OPTIONS;

	  if (XkbRF_GetNamesProp (xdisplay, &tmp, &vd) && tmp)
	  {
	      rules    = tmp;
	      model    = vd.model;
	      layout   = vd.layout;
	      variants = vd.variant;
	      options  = vd.options;
	  }
	  else
	  {
	      ErrorF ("Couldn't interpret %s property\n",
		      _XKB_RF_NAMES_PROP_ATOM);
	      ErrorF ("Use defaults: rules - '%s' model - '%s' layout - '%s'\n",
		      rules, model, layout);
	  }

	  desc = XkbGetKeyboard (xdisplay,
				 XkbGBN_AllComponentsMask,
				 XkbUseCoreKbd);

	  if (desc && desc->geom)
	  {
	      XkbComponentNamesRec names;

	      XkbGetControls (xdisplay, XkbAllControlsMask, desc);

	      memset (&names, 0, sizeof (XkbComponentNamesRec));

	      if (XkbInitialMap)
	      {
		  if ((names.keymap = strchr (XkbInitialMap, '/')) != NULL)
		      names.keymap++;
		  else
		      names.keymap = XkbInitialMap;
	      }

	      XkbSetRulesDflts (rules, model, layout, variants, options);

	      ret = XkbInitKeyboardDeviceStruct ((pointer) pDev,
						 &names,
						 &xglxKeySyms,
						 xglxModMap,
						 xglxBell,
						 xglxKbdCtrl);

	      if (ret)
		  XkbDDXChangeControls ((pointer) pDev, desc->ctrls,
					desc->ctrls);

	      XkbFreeKeyboard (desc, 0, False);
	  }

	  if (ret)
	  {
	      desc = XkbAllocKeyboard ();
	      if (desc)
	      {
		  XkbGetIndicatorMap (xdisplay, XkbAllIndicatorsMask, desc);

		  for (i = 0; i < XkbNumIndicators; i++)
		      if (desc->indicators->phys_indicators & (1 << i))
			  desc->indicators->maps[i].flags = XkbIM_NoAutomatic;

		  XkbSetIndicatorMap (xdisplay, ~0, desc);
		  XkbFreeKeyboard (desc, 0, True);
	      }

	      XkbChangeEnabledControls (xdisplay,
					XkbUseCoreKbd,
					XkbAudibleBellMask,
					XkbAudibleBellMask);
	  }
      }
#endif

      if (!ret)
      {
	  XGetKeyboardControl (xdisplay, &values);

	  memmove (defaultKeyboardControl.autoRepeats,
		   values.auto_repeats, sizeof (values.auto_repeats));

	  ret = InitKeyboardDeviceStruct (pDev,
					  &xglxKeySyms,
					  xglxModMap,
					  xglxBell,
					  xglxKbdCtrl);
      }

#ifdef _XSERVER64
      xfree (xkeyMap);
#else
      XFree (xkeyMap);
#endif

      if (!ret)
	  return BadImplementation;

    } break;
    case DEVICE_ON:
	pDev->on = TRUE;
	break;
    case DEVICE_OFF:
    case DEVICE_CLOSE:
	pDev->on = FALSE;
	break;
    }

    return Success;
}

Bool
xglxLegalModifier (unsigned int key,
		   DevicePtr    pDev)
{
    return TRUE;
}

static void
xglxChangePointerControl (DeviceIntPtr pDev,
			  PtrCtrl      *ctrl)
{
    XChangePointerControl (xdisplay, TRUE, TRUE,
			   ctrl->num, ctrl->den, ctrl->threshold);
}

static int
xglxPointerProc (DeviceIntPtr pDevice,
		 int	      onoff)
{
    BYTE      map[MAX_BUTTONS + 1];
    DevicePtr pDev = (DevicePtr) pDevice;
    int       i, nMap;

    switch (onoff) {
    case DEVICE_INIT:
	nMap = XGetPointerMapping (xdisplay, map, MAX_BUTTONS);
	for (i = 0; i <= nMap; i++)
	    map[i] = i;

	InitPointerDeviceStruct (pDev, map, nMap,
				 miPointerGetMotionEvents,
				 xglxChangePointerControl,
				 miPointerGetMotionBufferSize ());
	break;
    case DEVICE_ON:
	pDev->on = TRUE;
	break;
    case DEVICE_OFF:
    case DEVICE_CLOSE:
	pDev->on = FALSE;
	break;
    }

    return Success;
}

void
xglxProcessInputEvents (void)
{
    mieqProcessInputEvents ();
    miPointerUpdate ();
}

void
xglxInitInput (int  argc,
	       char **argv)
{
    DeviceIntPtr pKeyboard, pPointer;

    pPointer  = AddInputDevice (xglxPointerProc, TRUE);
    pKeyboard = AddInputDevice (xglxKeybdProc, TRUE);

    RegisterPointerDevice (pPointer);
    RegisterKeyboardDevice (pKeyboard);

    miRegisterPointerDevice (screenInfo.screens[0], pPointer);
    mieqInit (&pKeyboard->public, &pPointer->public);

    AddEnabledDevice (XConnectionNumber (xdisplay));

    RegisterBlockAndWakeupHandlers (xglxBlockHandler,
				    xglxWakeupHandler,
				    NULL);
}

void
xglxUseMsg (void)
{
    ErrorF ("-screen WIDTH[/WIDTHMM]xHEIGHT[/HEIGHTMM] "
	    "specify screen characteristics\n");
    ErrorF ("-fullscreen            run fullscreen\n");
    ErrorF ("-display string        display name of the real server\n");
    ErrorF ("-softcursor            force software cursor\n");
    ErrorF ("-scrns num             number of screens to generate\n");
    ErrorF ("-primary num           xinerama screen to use as first screen\n");

    if (!xDisplayName)
	xglxUseXorgMsg ();
}

int
xglxProcessArgument (int  argc,
		     char **argv,
		     int  i)
{
    static Bool checkDisplayName = FALSE;

    if (!checkDisplayName)
    {
	char *display = ":0";
	int  j;

	for (j = i; j < argc; j++)
	{
	    if (!strcmp (argv[j], "-display"))
	    {
		if (++j < argc)
		    xDisplayName = argv[j];

		break;
	    }
	    else if (argv[j][0] == ':')
	    {
		display = argv[j];
	    }
	}

	if (!xDisplayName)
	    xDisplayName = getenv ("DISPLAY");

	if (xDisplayName)
	{
	    int n;

	    n = strspn (xDisplayName, ":0123456789");
	    if (strncmp (xDisplayName, display, n) == 0)
		xDisplayName = 0;
	}

	if (xDisplayName)
	    fullscreen = FALSE;

	displayOffset = atoi (display + 1);

	checkDisplayName = TRUE;
    }

    if (!strcmp (argv[i], "-screen"))
    {
	if ((i + 1) < argc)
	{
	    xglParseScreen (argv[i + 1]);
	}
	else
	    return 1;

	return 2;
    }
    else if (!strcmp (argv[i], "-fullscreen"))
    {
	fullscreen = TRUE;
	return 1;
    }
    else if (!strcmp (argv[i], "-display"))
    {
	if (++i < argc)
	    return 2;

	return 0;
    }
    else if (!strcmp (argv[i], "-softcursor"))
    {
	softCursor = TRUE;
	return 1;
    }
    else if (!strcmp (argv[i], "-scrns"))
    {
	if ((i + 1) < argc)
	{
	    int n;

	    n = atoi (argv[i + 1]);
	    if (n > 1 && n <= MAXSCREENS)
		numScreen = n;
	}
	else
	    return 1;

	return 2;
    }
    else if (!strcmp (argv[i], "-primary"))
    {
	if ((i + 1) < argc)
	{
	    primaryScreen = atoi (argv[i + 1]);
	}
	else
	    return 1;

	return 2;
    }
    else if (!xDisplayName)
    {
	return xglxProcessXorgArgument (argc, argv, i);
    }

    return 0;
}

void
xglxAbort (void)
{
    xglxAbortXorg ();
}

void
xglxGiveUp (void)
{
    AbortDDX ();
}

void
xglxOsVendorInit (void)
{
    miRegionInit (&screenRegion, NULL, 0);

    if (!xdisplay)
    {
	char *name = xDisplayName;

	if (!name)
	    name = xglxInitXorg (displayOffset);

	xdisplay = XOpenDisplay (name);
	if (!xdisplay)
	    FatalError ("can't open display: %s\n", name ? name : "NULL");

	xscreen = DefaultScreen (xdisplay);

	if (!xDisplayName)
	{
	    int timeout, interval, preferBlanking, allowExposures;

	    XDefineCursor (xdisplay, RootWindow (xdisplay, xscreen),
			   XCreateFontCursor (xdisplay, XC_watch));

	    if (DPMSCapable (xdisplay))
	    {
		CARD16 standby, suspend, off;

		DPMSGetTimeouts (xdisplay, &standby, &suspend, &off);
		DPMSSetTimeouts (xdisplay, 0, 0, 0);
		DPMSEnable (xdisplay);
		DPMSForceLevel (xdisplay, DPMSModeOn);

		DPMSStandbyTime = standby * MILLI_PER_SECOND;
		DPMSSuspendTime = suspend * MILLI_PER_SECOND;
		DPMSOffTime     = off     * MILLI_PER_SECOND;

		xDpms = TRUE;
	    }

	    XGetScreenSaver (xdisplay, &timeout, &interval,
			     &preferBlanking, &allowExposures);
	    XSetScreenSaver (xdisplay, 0, interval,
			     preferBlanking, allowExposures);
	    XResetScreenSaver (xdisplay);

	    if (XineramaIsActive (xdisplay)

#ifdef PANORAMIX
		&& noPanoramiXExtension
#endif

		)
	    {
		XineramaScreenInfo *info;
		int		   nInfo = 0;
		RegionRec	   region;
		BoxRec		   box;
		BoxPtr		   rect;
		int		   i;

		info = XineramaQueryScreens (xdisplay, &nInfo);

		for (i = 0; i < nInfo; i++)
		{
		    box.x1 = info[i].x_org;
		    box.y1 = info[i].y_org;
		    box.x2 = box.x1 + info[i].width;
		    box.y2 = box.y1 + info[i].height;

		    if (miRectIn (&screenRegion, &box) == rgnOUT)
		    {
			rect = Xrealloc (screenRect,
					 sizeof (BoxRec) * (nScreenRect + 1));
			if (!rect)
			    continue;

			if (nScreenRect &&
			    info[i].screen_number == primaryScreen)
			{
			    memmove (rect + 1, rect,
				     sizeof (BoxRec) * nScreenRect);
			    *rect = box;
			}
			else
			{
			    rect[nScreenRect] = box;
			    screenRect = rect;
			}

			nScreenRect++;

			miRegionInit (&region, &box, 1);
			miUnion (&screenRegion, &screenRegion, &region);
			miRegionUninit (&region);
		    }
		}
	    }

	    if (nScreenRect > 1 && nScreenRect <= MAXSCREENS)
		numScreen = nScreenRect;
	}

	if (!glitz_glx_find_window_format (xdisplay, xscreen, 0, NULL, 0))
	    FatalError ("no GLX visuals available\n");
    }
}

#ifdef ARGB_CURSOR

#include <X11/extensions/Xrender.h>

static Bool
xglxARGBCursorSupport (void)
{
    int renderMajor, renderMinor;

    if (!XRenderQueryVersion (xdisplay, &renderMajor, &renderMinor))
	renderMajor = renderMinor = -1;

    return (renderMajor > 0 || renderMinor > 4);
}

static Cursor
xglxCreateARGBCursor (ScreenPtr pScreen,
		      CursorPtr pCursor)
{
    Pixmap	      xpixmap;
    XlibGC	      xgc;
    XImage	      *ximage;
    XRenderPictFormat *xformat;
    Picture	      xpicture;
    Cursor	      cursor;

    XGLX_SCREEN_PRIV (pScreen);

    xpixmap = XCreatePixmap (xdisplay,
			     pScreenPriv->win,
			     pCursor->bits->width,
			     pCursor->bits->height,
			     32);

    xgc = XCreateGC (xdisplay, xpixmap, 0, NULL);

    ximage = XCreateImage (xdisplay,
			   DefaultVisual (xdisplay, xscreen),
			   32, ZPixmap, 0,
			   (char *) pCursor->bits->argb,
			   pCursor->bits->width,
			   pCursor->bits->height,
			   32, pCursor->bits->width * 4);

    XPutImage (xdisplay, xpixmap, xgc, ximage,
	       0, 0, 0, 0, pCursor->bits->width, pCursor->bits->height);

    XFree (ximage);
    XFreeGC (xdisplay, xgc);

    xformat = XRenderFindStandardFormat (xdisplay, PictStandardARGB32);
    xpicture = XRenderCreatePicture (xdisplay, xpixmap, xformat, 0, 0);

    cursor = XRenderCreateCursor (xdisplay, xpicture,
				  pCursor->bits->xhot,
				  pCursor->bits->yhot);

    XRenderFreePicture (xdisplay, xpicture);
    XFreePixmap (xdisplay, xpixmap);

    return cursor;
}

#endif

Bool
xglxDPMSSupported (void)
{
    return xDpms;
}

void
xglxDPMSSet (int level)
{
    if (!xDpms)
	return;

    if (level < 0)
	level = DPMSModeOn;

    if (level > 3)
	level = DPMSModeOff;

    DPMSPowerLevel = level;

    DPMSForceLevel (xdisplay, level);
}
