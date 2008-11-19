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
 *   Rickard E. (Rik) Faith <faith@redhat.com>
 *
 */

/** \file
 * Provide expected functions for initialization from the ddx layer and
 * global variables for the DMX server. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxinit.h"
#include "dmxsync.h"
#include "dmxlog.h"
#include "dmxinput.h"
#include "dmxscrinit.h"
#include "dmxcursor.h"
#include "dmxfont.h"
#include "dmxcb.h"
#include "dmxprop.h"
#include "dmxstat.h"
#include "dmxlaunch.h"
#include "dmxgrab.h"
#include "dmxselection.h"
#include "dmxshm.h"
#ifdef RENDER
#include "dmxpict.h"
#endif
#ifdef COMPOSITE
#include "dmxcomp.h"
#endif
#include "dmxextension.h"

#include <X11/Xos.h>                /* For gettimeofday */
#include "dixstruct.h"
#include "opaque.h"
#include "panoramiXsrv.h"

#ifdef HAVE_SHA1_IN_LIBMD /* Use libmd for SHA1 */
# include <sha1.h>
#else /* Use OpenSSL's libcrypto */
# include <stddef.h>  /* buggy openssl/sha.h wants size_t */
# include <openssl/sha.h>
#endif

#include <signal.h>             /* For SIGQUIT */

#ifdef GLXEXT
#include <GL/glx.h>
#include <GL/glxint.h>
#include "dmx_glxvisuals.h"
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

extern void GlxSetVisualConfigs(
    int               nconfigs,
    __GLXvisualConfig *configs,
    void              **configprivs
);
#endif /* GLXEXT */

/* Global variables available to all Xserver/hw/dmx routines. */
int             dmxNumScreens;
DMXScreenInfo  *dmxScreens;

XErrorEvent     dmxLastErrorEvent;
Bool            dmxErrorOccurred = FALSE;

char           *dmxFontPath = NULL;

Bool            dmxOffScreenOpt = FALSE;

Bool            dmxSubdividePrimitives = TRUE;

Bool            dmxLazyWindowCreation = FALSE;

Bool            dmxUseXKB = TRUE;

int             dmxDepth = 0;

#ifndef GLXEXT
static Bool     dmxGLXProxy = FALSE;
#else
Bool            dmxGLXProxy = FALSE;

Bool            dmxGLXSwapGroupSupport = TRUE;

Bool            dmxGLXSyncSwap = FALSE;

Bool            dmxGLXFinishSwap = FALSE;
#endif

Bool            dmxIgnoreBadFontPaths = FALSE;

Bool            dmxAddRemoveScreens = TRUE;

int             dmxLaunchIndex = 0;
char            *dmxLaunchVT = NULL;

int             dmxNumDetached = 4;

#ifdef RANDR
int             xRROutputsPerScreen = 1;
int             xRRCrtcsPerScreen = 1;
#endif

DMXPropTrans    *dmxPropTrans = NULL;
int             dmxPropTransNum = 0;

DMXSelectionMap *dmxSelectionMap = NULL;
int             dmxSelectionMapNum = 0;

#ifdef XV
char            **dmxXvImageFormats = NULL;
int             dmxXvImageFormatsNum = 0;
#endif

char            dmxDigest[64];

static void
dmxSigHandler (int signo)
{
    signal (signo, SIG_IGN);
    xorg_backtrace ();
    FatalError ("Caught signal %d.  Server aborting\n", signo);
}

/* dmxErrorHandler catches errors that occur when calling one of the
 * back-end servers.  Some of this code is based on _XPrintDefaultError
 * in xc/lib/X11/XlibInt.c */
static int dmxErrorHandler(Display *dpy, XErrorEvent *ev)
{
#define DMX_ERROR_BUF_SIZE 256
                                /* RATS: these buffers are only used in
                                 * length-limited calls. */
    char        buf[DMX_ERROR_BUF_SIZE];
    char        request[DMX_ERROR_BUF_SIZE];
    _XExtension *ext = NULL;

    dmxErrorOccurred  = TRUE;
    dmxLastErrorEvent = *ev;

    XGetErrorText(dpy, ev->error_code, buf, sizeof(buf));
    dmxLog(dmxWarning, "dmxErrorHandler: %s\n", buf);

                                /* Find major opcode name */
    if (ev->request_code < 128) {
        XmuSnprintf(request, sizeof(request), "%d", ev->request_code);
        XGetErrorDatabaseText(dpy, "XRequest", request, "", buf, sizeof(buf));
    } else {
        for (ext = dpy->ext_procs;
             ext && ext->codes.major_opcode != ev->request_code;
             ext = ext->next);
        if (ext) strncpy(buf, ext->name, sizeof(buf));
        else     buf[0] = '\0';
    }
    dmxLog(dmxWarning, "                 Major opcode: %d (%s)\n",
           ev->request_code, buf);

                                /* Find minor opcode name */
    if (ev->request_code >= 128 && ext) {
        XmuSnprintf(request, sizeof(request), "%d", ev->request_code);
        XmuSnprintf(request, sizeof(request), "%s.%d",
                    ext->name, ev->minor_code);
        XGetErrorDatabaseText(dpy, "XRequest", request, "", buf, sizeof(buf));
        dmxLog(dmxWarning, "                 Minor opcode: %d (%s)\n",
               ev->minor_code, buf);
    }

                                /* Provide value information */
    switch (ev->error_code) {
    case BadValue:
        dmxLog(dmxWarning, "                 Value:        0x%x\n",
               ev->resourceid);
        break;
    case BadAtom:
        dmxLog(dmxWarning, "                 AtomID:       0x%x\n",
               ev->resourceid);
        break;
    default:
        dmxLog(dmxWarning, "                 ResourceID:   0x%x\n",
               ev->resourceid);
        break;
    }

                                /* Provide serial number information */
    dmxLog(dmxWarning, "                 Failed serial number:  %d\n",
           ev->serial);
    dmxLog(dmxWarning, "                 Current serial number: %d\n",
           dpy->request);

//    abort ();

    return 0;
}

int     _dmx_jumpbuf_set = 0;
int     _dmx_io_error = 0;
jmp_buf _dmx_jumpbuf;

static int dmxIOErrorHandler (Display *dpy)
{
    _dmx_io_error++;

    if (!_dmx_jumpbuf_set)
    {
	ErrorF ("_dmx_jumpbuf not set\n");
	abort ();
    }
    else
    {
	longjmp (_dmx_jumpbuf, 1);
    }

    return 0;
}

#ifdef GLXEXT
static int dmxNOPErrorHandler(Display *dpy, XErrorEvent *ev)
{
    return 0;
}
#endif

char *
dmxMemDup (const char *data,
	   int        dataLen)
{
    char *d;

    d = malloc (dataLen);
    if (!d)
	return NULL;

    memcpy (d, data, dataLen);

    return d;
}

DMXScreenInfo *
dmxAddScreen(const char *name,
	     const char *display,
	     const char *authType,
	     int        authTypeLen,
	     const char *authData,
	     int        authDataLen,
	     int        virtualFb)
{
    DMXScreenInfo *dmxScreen;
    
    if (!(dmxScreens = realloc(dmxScreens,
                               (dmxNumScreens+1) * sizeof(*dmxScreens))))
        dmxLog(dmxFatal,
               "dmxAddScreen: realloc failed for screen %d (%s)\n",
               dmxNumScreens, name);

    dmxScreen = &dmxScreens[dmxNumScreens];
    memset(dmxScreen, 0, sizeof(*dmxScreen));
    dmxScreen->name        = strdup (name);
    dmxScreen->display     = strdup (display);
    dmxScreen->index       = dmxNumScreens;
    dmxScreen->scrnWidth   = 0;
    dmxScreen->scrnHeight  = 0;
    dmxScreen->rootX       = 0;
    dmxScreen->rootY       = 0;
    dmxScreen->stat        = dmxStatAlloc();
    dmxScreen->authType    = dmxMemDup (authType, authTypeLen);
    dmxScreen->authTypeLen = authTypeLen;
    dmxScreen->authData    = dmxMemDup (authData, authDataLen);
    dmxScreen->authDataLen = authDataLen;
    dmxScreen->virtualFb   = virtualFb;
    ++dmxNumScreens;

    return dmxScreen;
}

Bool dmxOpenDisplay(DMXScreenInfo *dmxScreen,
		    const char    *display,
		    const char    *authType,
		    int           authTypeLen,
		    const char    *authData,
		    int           authDataLen)
{
    dmxScreen->beDisplay = NULL;

    if (!display || !*display)
	return FALSE;

    if (authType && *authType)
	XSetAuthorization ((char *) authType, authTypeLen,
			   (char *) authData, authDataLen);

    dmxScreen->alive = 1;

    XLIB_PROLOGUE (dmxScreen);
    dmxScreen->beDisplay = XOpenDisplay (display);
    XLIB_EPILOGUE (dmxScreen);

    if (!dmxScreen->beDisplay)
    {
        dmxScreen->alive = 0;	
	return FALSE;
    }

    dmxScreen->alive      = 1;
    dmxScreen->broken     = 0;
    dmxScreen->inDispatch = FALSE;
    dmxScreen->fd         = XConnectionNumber (dmxScreen->beDisplay);
    dmxScreen->connection = XGetXCBConnection (dmxScreen->beDisplay);

    XSetEventQueueOwner (dmxScreen->beDisplay, XCBOwnsEventQueue);

    dmxScreen->sync.sequence = 0;

    AddEnabledDevice (dmxScreen->fd);

    dmxScreen->atomTable     = NULL;
    dmxScreen->atomTableSize = 0;

    dmxScreen->beAtomTable     = NULL;
    dmxScreen->beAtomTableSize = 0;

    return TRUE;
}

void dmxCloseDisplay(DMXScreenInfo *dmxScreen)
{
    RemoveEnabledDevice (dmxScreen->fd);

    if (dmxScreen->atomTable)
	xfree (dmxScreen->atomTable);
    if (dmxScreen->beAtomTable)
	xfree (dmxScreen->beAtomTable);

    xcb_disconnect (dmxScreen->connection);

    dmxScreen->alive = 0;
}

void dmxSetErrorHandler(DMXScreenInfo *dmxScreen)
{
    XSetErrorHandler(dmxErrorHandler);
    XSetIOErrorHandler(dmxIOErrorHandler);
}

static void dmxPrintScreenInfo(DMXScreenInfo *dmxScreen)
{
    XWindowAttributes attribs;
    int               ndepths = 0, *depths = NULL;
    int               i;
    Display           *dpy   = dmxScreen->beDisplay;
    Screen            *s     = DefaultScreenOfDisplay(dpy);
    int               scr    = DefaultScreen(dpy);

    XGetWindowAttributes(dpy, DefaultRootWindow(dpy), &attribs);
    if (!(depths = XListDepths(dpy, scr, &ndepths))) ndepths = 0;
    
    dmxLogOutput(dmxScreen, "Name of display: %s\n", DisplayString(dpy));
    dmxLogOutput(dmxScreen, "Version number:  %d.%d\n",
                 ProtocolVersion(dpy), ProtocolRevision(dpy));
    dmxLogOutput(dmxScreen, "Vendor string:   %s\n", ServerVendor(dpy));
    if (!strstr(ServerVendor(dpy), "XFree86")) {
        dmxLogOutput(dmxScreen, "Vendor release:  %d\n", VendorRelease(dpy));
    } else {
                                /* This code based on xdpyinfo.c */
    	int v = VendorRelease(dpy);
        int major = -1, minor = -1, patch = -1, subpatch = -1;

        if (v < 336)
            major = v / 100, minor = (v / 10) % 10, patch = v % 10;
        else if (v < 3900) {
            major = v / 1000;
            minor = (v / 100) % 10;
            if (((v / 10) % 10) || (v % 10)) {
                patch = (v / 10) % 10;
                if (v % 10) subpatch = v % 10;
            }
        } else if (v < 40000000) {
            major = v / 1000;
            minor = (v / 10) % 10;
            if (v % 10) patch = v % 10;
	} else {
            major = v / 10000000;
            minor = (v / 100000) % 100;
            patch = (v / 1000) % 100;
            if (v % 1000) subpatch = v % 1000;
	}
        dmxLogOutput(dmxScreen, "Vendor release:  %d (XFree86 version: %d.%d",
                     v, major, minor);
        if (patch > 0)    dmxLogOutputCont(dmxScreen, ".%d", patch);
        if (subpatch > 0) dmxLogOutputCont(dmxScreen, ".%d", subpatch);
        dmxLogOutputCont(dmxScreen, ")\n");
    }

    
    dmxLogOutput(dmxScreen, "Dimensions:      %dx%d pixels\n", 
                 attribs.width, attribs.height);
    dmxLogOutput(dmxScreen, "%d depths on screen %d: ", ndepths, scr);
    for (i = 0; i < ndepths; i++)
        dmxLogOutputCont(dmxScreen, "%c%d", i ? ',' : ' ', depths[i]);
    dmxLogOutputCont(dmxScreen, "\n");
    dmxLogOutput(dmxScreen, "Depth of root window:  %d plane%s (%d)\n",
                 attribs.depth, attribs.depth == 1 ? "" : "s",
                 DisplayPlanes(dpy, scr));
    dmxLogOutput(dmxScreen, "Number of colormaps:   %d min, %d max\n",
                 MinCmapsOfScreen(s), MaxCmapsOfScreen(s));
    dmxLogOutput(dmxScreen, "Options: backing-store %s, save-unders %s\n",
                 (DoesBackingStore (s) == NotUseful) ? "no" :
                 ((DoesBackingStore (s) == Always) ? "yes" : "when mapped"),
                 DoesSaveUnders (s) ? "yes" : "no");
    XFree(depths);
}

void dmxGetScreenAttribs(DMXScreenInfo *dmxScreen)
{
    XWindowAttributes attribs;
    Display           *dpy   = dmxScreen->beDisplay;
#ifdef GLXEXT
    int               dummy;
#endif

    XGetWindowAttributes(dpy, DefaultRootWindow(dpy), &attribs);

    dmxScreen->beWidth  = attribs.width;
    dmxScreen->beHeight = attribs.height;

    /* FIXME: Get these from the back-end server */
    dmxScreen->beXDPI = 96;
    dmxScreen->beYDPI = 96;

    dmxScreen->beDepth  = attribs.depth; /* FIXME: verify that this
					  * works always.  In
					  * particular, this will work
					  * well for depth=16, will fail
					  * because of colormap issues
					  * at depth 8.  More work needs
					  * to be done here. */

    if (dmxScreen->beDepth <= 8)       dmxScreen->beBPP = 8;
    else if (dmxScreen->beDepth <= 16) dmxScreen->beBPP = 16;
    else                               dmxScreen->beBPP = 32;

    if (dmxScreen->scrnWin != DefaultRootWindow(dpy))
        XGetWindowAttributes(dpy, dmxScreen->scrnWin, &attribs);
    
    dmxScreen->scrnWidth  = attribs.width;
    dmxScreen->scrnHeight = attribs.height;

#ifdef GLXEXT
    /* get the majorOpcode for the back-end GLX extension */
    XQueryExtension(dpy, "GLX", &dmxScreen->glxMajorOpcode,
		    &dummy, &dmxScreen->glxErrorBase);
#endif

    dmxPrintScreenInfo(dmxScreen);
    dmxLogOutput(dmxScreen, "%dx%d on %dx%d at depth=%d, bpp=%d\n",
                 dmxScreen->scrnWidth, dmxScreen->scrnHeight,
                 dmxScreen->beWidth, dmxScreen->beHeight,
                 dmxScreen->beDepth, dmxScreen->beBPP);
    if (dmxScreen->beDepth == 8)
        dmxLogOutputWarning(dmxScreen,
                            "Support for depth == 8 is not complete\n");
}

Bool dmxGetVisualInfo(DMXScreenInfo *dmxScreen)
{
    int i;
    XVisualInfo visinfo;

    visinfo.screen = DefaultScreen(dmxScreen->beDisplay);
    dmxScreen->beVisuals = XGetVisualInfo(dmxScreen->beDisplay,
					  VisualScreenMask,
					  &visinfo,
					  &dmxScreen->beNumVisuals);

    dmxScreen->beDefVisualIndex = -1;

    if (defaultColorVisualClass >= 0 || dmxDepth > 0) {
	for (i = 0; i < dmxScreen->beNumVisuals; i++)
	    if (defaultColorVisualClass >= 0) {
		if (dmxScreen->beVisuals[i].class == defaultColorVisualClass) {
		    if (dmxDepth > 0) {
			if (dmxScreen->beVisuals[i].depth == dmxDepth) {
			    dmxScreen->beDefVisualIndex = i;
			    break;
			}
		    } else {
			dmxScreen->beDefVisualIndex = i;
			break;
		    }
		}
	    } else if (dmxScreen->beVisuals[i].depth == dmxDepth) {
		dmxScreen->beDefVisualIndex = i;
		break;
	    }
    } else {
	visinfo.visualid =
	    XVisualIDFromVisual(DefaultVisual(dmxScreen->beDisplay,
					      visinfo.screen));

	for (i = 0; i < dmxScreen->beNumVisuals; i++)
	    if (visinfo.visualid == dmxScreen->beVisuals[i].visualid) {
		dmxScreen->beDefVisualIndex = i;
		break;
	    }
    }

    for (i = 0; i < dmxScreen->beNumVisuals; i++)
        dmxLogVisual(dmxScreen, &dmxScreen->beVisuals[i],
                     (i == dmxScreen->beDefVisualIndex));

    return (dmxScreen->beDefVisualIndex >= 0);
}

void dmxGetColormaps(DMXScreenInfo *dmxScreen)
{
    int i;

    dmxScreen->beNumDefColormaps = dmxScreen->beNumVisuals;
    dmxScreen->beDefColormaps = xalloc(dmxScreen->beNumDefColormaps *
				       sizeof(*dmxScreen->beDefColormaps));

    for (i = 0; i < dmxScreen->beNumDefColormaps; i++)
	dmxScreen->beDefColormaps[i] =
	    XCreateColormap(dmxScreen->beDisplay,
			    DefaultRootWindow(dmxScreen->beDisplay),
			    dmxScreen->beVisuals[i].visual,
			    AllocNone);

    dmxScreen->beBlackPixel = BlackPixel(dmxScreen->beDisplay,
					 DefaultScreen(dmxScreen->beDisplay));
    dmxScreen->beWhitePixel = WhitePixel(dmxScreen->beDisplay,
					 DefaultScreen(dmxScreen->beDisplay));
}

void dmxGetPixmapFormats(DMXScreenInfo *dmxScreen)
{
    dmxScreen->beDepths =
	XListDepths(dmxScreen->beDisplay, DefaultScreen(dmxScreen->beDisplay),
		    &dmxScreen->beNumDepths);

    dmxScreen->bePixmapFormats =
	XListPixmapFormats(dmxScreen->beDisplay,
			   &dmxScreen->beNumPixmapFormats);
}

static Bool dmxSetPixmapFormats(ScreenInfo *pScreenInfo,
				DMXScreenInfo *dmxScreen)
{
    XPixmapFormatValues *bePixmapFormat;
    PixmapFormatRec     *format;
    int                  i, j;

    pScreenInfo->imageByteOrder = ImageByteOrder(dmxScreen->beDisplay);
    pScreenInfo->bitmapScanlineUnit = BitmapUnit(dmxScreen->beDisplay);
    pScreenInfo->bitmapScanlinePad = BitmapPad(dmxScreen->beDisplay);
    pScreenInfo->bitmapBitOrder = BitmapBitOrder(dmxScreen->beDisplay);

    pScreenInfo->numPixmapFormats = 0;
    for (i = 0; i < dmxScreen->beNumPixmapFormats; i++) {
	bePixmapFormat = &dmxScreen->bePixmapFormats[i];
	for (j = 0; j < dmxScreen->beNumDepths; j++)
	    if ((bePixmapFormat->depth == 1) ||
		(bePixmapFormat->depth == dmxScreen->beDepths[j])) {
		format = &pScreenInfo->formats[pScreenInfo->numPixmapFormats];

		format->depth        = bePixmapFormat->depth;
		format->bitsPerPixel = bePixmapFormat->bits_per_pixel;
		format->scanlinePad  = bePixmapFormat->scanline_pad;

		pScreenInfo->numPixmapFormats++;
		break;
	    }
    }

    return TRUE;
}

/** Initialize the display and collect relevant information about the
 *  display properties */
static Bool dmxDisplayInit(DMXScreenInfo *dmxScreen)
{
    if (!dmxOpenDisplay(dmxScreen,
			dmxScreen->display,
			dmxScreen->authType,
			dmxScreen->authTypeLen,
			dmxScreen->authData,
			dmxScreen->authDataLen))
    {
	if (dmxScreen->display && *dmxScreen->display)
	    dmxLog(dmxWarning,
		   "dmxOpenDisplay: Unable to open display %s\n",
		   dmxScreen->display);

	dmxScreen->scrnWidth  = 1;
	dmxScreen->scrnHeight = 1;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    dmxScreen->scrnWidth  = dmxScreens[0].scrnWidth;
	    dmxScreen->scrnHeight = dmxScreens[0].scrnHeight;
	}
#endif

	dmxScreen->beWidth    = 1;
	dmxScreen->beHeight   = 1;
	dmxScreen->beXDPI     = 96;
	dmxScreen->beYDPI     = 96;
	dmxScreen->beDepth    = 24;
	dmxScreen->beBPP      = 32;

	return FALSE;
    }
    else
    {
	if (!dmxScreen->scrnWin)
	    dmxScreen->scrnWin = DefaultRootWindow (dmxScreen->beDisplay);
	    
	dmxSetErrorHandler(dmxScreen);
	dmxGetScreenAttribs(dmxScreen);

	if (!dmxGetVisualInfo(dmxScreen))
	{
	    dmxLog(dmxWarning,
		   "dmxGetVisualInfo: No matching visuals found\n");

	    XLIB_PROLOGUE (dmxScreen);
	    XCloseDisplay(dmxScreen->beDisplay);
	    XLIB_EPILOGUE (dmxScreen);
	    dmxScreen->beDisplay = NULL;

	    return FALSE;
	}
	else
	{
	    dmxGetColormaps(dmxScreen);
	    dmxGetPixmapFormats(dmxScreen);
	}
    }

    return TRUE;
}

/* If this doesn't compile, just add || defined(yoursystem) to the line
 * below.  This information is to help with bug reports and is not
 * critical. */
#if !defined(_POSIX_SOURCE) 
static const char *dmxExecOS(void) { return ""; }
#else
#include <sys/utsname.h>
static const char *dmxExecOS(void)
{
    static char buffer[128];
    static int  initialized = 0;
    struct utsname u;

    if (!initialized++) {
        memset(buffer, 0, sizeof(buffer));
        uname(&u);
        XmuSnprintf(buffer, sizeof(buffer)-1, "%s %s %s",
                    u.sysname, u.release, u.version);
    }
    return buffer;
}
#endif

static const char *dmxBuildCompiler(void)
{
    static char buffer[128];
    static int  initialized = 0;

    if (!initialized++) {
        memset(buffer, 0, sizeof(buffer));
#if defined(__GNUC__) && defined(__GNUC_MINOR__) &&defined(__GNUC_PATCHLEVEL__)
        XmuSnprintf(buffer, sizeof(buffer)-1, "gcc %d.%d.%d",
                    __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
    }
    return buffer;
}

static const char *dmxExecHost(void)
{
    static char buffer[128];
    static int  initialized = 0;

    if (!initialized++) {
        memset(buffer, 0, sizeof(buffer));
        XmuGetHostname(buffer, sizeof(buffer) - 1);
    }
    return buffer;
}

/** This routine is called in Xserver/dix/main.c from \a main(). */
void InitOutput(ScreenInfo *pScreenInfo, int argc, char *argv[])
{
    int                  i, nDetached;
    static unsigned long dmxGeneration = 0;
#ifdef GLXEXT
    Bool                 glxSupported  = TRUE;
#endif

    if (dmxGeneration != serverGeneration) {
	int vendrel = VENDOR_RELEASE;
        int major, minor, year, month, day;
	unsigned char sha1[20];
	const char *host = dmxExecHost ();
	time_t t = time (NULL);
	pid_t pid = getpid ();

#ifdef HAVE_SHA1_IN_LIBMD /* Use libmd for SHA1 */
	SHA1_CTX ctx;

	SHA1Init (&ctx);
	SHA1Update (&ctx, display, strlen (display));
	SHA1Update (&ctx, host, strlen (host));
	SHA1Update (&ctx, &t, sizeof (time_t));
	SHA1Update (&ctx, &pid, sizeof (pid_t));
	SHA1Final (sha1, &ctx);
#else /* Use OpenSSL's libcrypto */
	SHA_CTX ctx;

	if (!SHA1_Init (&ctx)                              ||
	    !SHA1_Update (&ctx, display, strlen (display)) ||
	    !SHA1_Update (&ctx, host, strlen (host))       ||
	    !SHA1_Update (&ctx, &t, sizeof (time_t))       ||
	    !SHA1_Update (&ctx, &pid, sizeof (pid_t))      ||
	    !SHA1_Final (sha1, &ctx))
	{
	    dmxLog(dmxFatal, "SHA1_Init failed\n");
	}
#endif

	for (i = 0; i < sizeof (sha1); i++)
	    snprintf(dmxDigest + 2 * i, sizeof (dmxDigest) - 2 * i, "%02x",
		     sha1[i]);
        
        dmxGeneration = serverGeneration;

        major    = vendrel / 100000000;
        vendrel -= major   * 100000000;
        minor    = vendrel /   1000000;
        vendrel -= minor   *   1000000;
        year     = vendrel /     10000;
        vendrel -= year    *     10000;
        month    = vendrel /       100;
        vendrel -= month   *       100;
        day      = vendrel;

                                /* Add other epoch tests here */
        if (major > 0 && minor > 0) year += 2000;

        dmxLog(dmxInfo, "Generation:         %d\n", dmxGeneration);
	dmxLog(dmxInfo, "DMX digest:         %s\n", dmxDigest);
        dmxLog(dmxInfo, "DMX version:        %d.%d.%02d%02d%02d (%s)\n",
               major, minor, year, month, day, VENDOR_STRING);

        SetVendorRelease(VENDOR_RELEASE);
        SetVendorString(VENDOR_STRING);

        if (dmxGeneration == 1) {
            dmxLog(dmxInfo, "DMX Build OS:       %s (%s)\n", OSNAME, OSVENDOR);
            dmxLog(dmxInfo, "DMX Build Compiler: %s\n", dmxBuildCompiler());
            dmxLog(dmxInfo, "DMX Execution OS:   %s\n", dmxExecOS());
            dmxLog(dmxInfo, "DMX Execution Host: %s\n", dmxExecHost());
        }
        dmxLog(dmxInfo, "MAXSCREENS:         %d\n", MAXSCREENS);

        for (i = 0; i < dmxNumScreens; i++) {
            if (dmxScreens[i].beDisplay)
                dmxLog(dmxWarning, "Display \"%s\" still open\n",
                       dmxScreens[i].display);
            dmxStatFree(dmxScreens[i].stat);
            dmxScreens[i].stat = NULL;
        }
        if (dmxScreens) free(dmxScreens);
        dmxScreens    = NULL;
        dmxNumScreens = 0;
    }

    /* Make sure that the command-line arguments are sane. */
    if (dmxAddRemoveScreens && dmxGLXProxy) {
	/* Currently it is not possible to support GLX and Render
	 * extensions with dynamic screen addition/removal due to the
	 * state that each extension keeps, which cannot be restored. */
        dmxLog(dmxWarning,
	       "GLX Proxy and Render extensions do not yet support dynamic\n");
        dmxLog(dmxWarning,
	       "screen addition and removal.  Please specify -noglxproxy\n");
        dmxLog(dmxWarning,
	       "and -norender on the command line or in the configuration\n");
        dmxLog(dmxWarning,
	       "file to disable these two extensions if you wish to use\n");
        dmxLog(dmxWarning,
	       "the dynamic addition and removal of screens support.\n");
        dmxLog(dmxFatal,
	       "Dynamic screen addition/removal error (see above).\n");
    }

    for (i = 0; i < dmxPropTransNum; i++)
	dmxPropTrans[i].type = MakeAtom ((char *) dmxPropTrans[i].name,
					 strlen (dmxPropTrans[i].name),
					 TRUE);

    for (i = 0; i < dmxSelectionMapNum; i++)
    {
	char *beName;

	dmxSelectionMap[i].atom = MakeAtom ((char *) dmxSelectionMap[i].name,
					    strlen (dmxSelectionMap[i].name),
					    TRUE);

	beName = xalloc (strlen (dmxSelectionMap[i].name) +
			 strlen (dmxDigest) + 2);
	if (!beName)
	    dmxLog (dmxFatal, "InitOutput: not enough memory\n");
	
	sprintf (beName, "%s_%s", dmxSelectionMap[i].name, dmxDigest);
	dmxSelectionMap[i].beAtom = MakeAtom ((char *) beName,
					      strlen (beName),
					      TRUE);
	xfree (beName);
    }

    if (!dmxNumScreens)
    {
	dmxLaunchDisplay (argc, argv, dmxLaunchIndex, dmxLaunchVT);
	if (!dmxNumScreens)
	    dmxLog(dmxFatal, "InitOutput: no back-end displays found\n");
    }

    nDetached = dmxNumDetached;
    if (dmxNumScreens + nDetached > MAXSCREENS)
	nDetached = MAXSCREENS - dmxNumScreens;
    
    if (nDetached > 0)
    {
	dmxLog (dmxInfo, "Adding %d detached displays\n", nDetached);

	while (nDetached--)
	    dmxAddScreen ("", "", NULL, 0, NULL, 0, 0);
    }
    
    /* Disable lazy window creation optimization if offscreen
     * optimization is disabled */
    if (!dmxOffScreenOpt && dmxLazyWindowCreation) {
        dmxLog(dmxInfo,
	       "InitOutput: Disabling lazy window creation optimization\n");
        dmxLog(dmxInfo,
	       "            since it requires the offscreen optimization\n");
        dmxLog(dmxInfo,
	       "            to function properly.\n");
	dmxLazyWindowCreation = FALSE;
    }

    /* Open each display and gather information about it. */
    for (i = 0; i < dmxNumScreens; i++)
	if (!dmxDisplayInit(&dmxScreens[i]) && i == 0)
	    dmxLog(dmxFatal,
		   "dmxOpenDisplay: Unable to open display %s\n",
		   dmxScreens[i].display);

#if PANORAMIX
    /* Register a Xinerama callback which will run from within
     * PanoramiXCreateConnectionBlock.  We can use the callback to
     * determine if Xinerama is loaded and to check the visuals
     * determined by PanoramiXConsolidate. */
    XineramaRegisterConnectionBlockCallback(dmxConnectionBlockCallback);
#endif

    /* Since we only have a single screen thus far, we only need to set
       the pixmap formats to match that screen.  FIXME: this isn't true.*/
    if (!dmxSetPixmapFormats(pScreenInfo, &dmxScreens[0])) return;

    /* Might want to install a signal handler to allow cleaning up after
     * unexpected signals.  The DIX/OS layer already handles SIGINT and
     * SIGTERM, so everything is OK for expected signals. --DD
     *
     * SIGHUP, SIGINT, and SIGTERM are trapped in os/connection.c
     * SIGQUIT is another common signal that is sent from the keyboard.
     * Trap it here, to ensure that the keyboard modifier map and other
     * state for the input devices are restored. (This makes the
     * behavior of SIGQUIT somewhat unexpected, since it will be the
     * same as the behavior of SIGINT.  However, leaving the modifier
     * map of the input devices empty is even more unexpected.) --RF
     */
    OsSignal (SIGQUIT, GiveUp);
    OsSignal (SIGSEGV, dmxSigHandler);
    OsSignal (SIGILL,  dmxSigHandler);
    OsSignal (SIGFPE,  dmxSigHandler);
    OsSignal (SIGABRT, dmxSigHandler);

#ifdef GLXEXT
    /* Check if GLX extension exists on all back-end servers */
    for (i = 0; i < dmxNumScreens; i++)
	glxSupported &= (dmxScreens[i].glxMajorOpcode > 0);
#endif

    /* Tell dix layer about the backend displays */
    for (i = 0; i < dmxNumScreens; i++) {

#ifdef GLXEXT
	if (glxSupported) {
	    /*
	     * Builds GLX configurations from the list of visuals
	     * supported by the back-end server, and give that
	     * configuration list to the glx layer - so that he will
	     * build the visuals accordingly.
	     */

	    DMXScreenInfo       *dmxScreen    = &dmxScreens[i];
	    __GLXvisualConfig   *configs      = NULL;
	    dmxGlxVisualPrivate **configprivs = NULL;
	    int                 nconfigs      = 0;
	    int                 (*oldErrorHandler)(Display *, XErrorEvent *);
	    int                 i;

	    /* Catch errors if when using an older GLX w/o FBconfigs */
	    oldErrorHandler = XSetErrorHandler(dmxNOPErrorHandler);

	    /* Get FBConfigs of the back-end server */
	    dmxScreen->fbconfigs = GetGLXFBConfigs(dmxScreen->beDisplay,
						   dmxScreen->glxMajorOpcode,
						   &dmxScreen->numFBConfigs);

	    XSetErrorHandler(oldErrorHandler);

	    dmxScreen->glxVisuals = 
		GetGLXVisualConfigs(dmxScreen->beDisplay,
				    DefaultScreen(dmxScreen->beDisplay),
				    &dmxScreen->numGlxVisuals);

	    if (dmxScreen->fbconfigs) {
		configs =
		    GetGLXVisualConfigsFromFBConfigs(dmxScreen->fbconfigs,
						     dmxScreen->numFBConfigs,
						     dmxScreen->beVisuals,
						     dmxScreen->beNumVisuals,
						     dmxScreen->glxVisuals,
						     dmxScreen->numGlxVisuals,
						     &nconfigs);
	    } else {
		configs = dmxScreen->glxVisuals;
		nconfigs = dmxScreen->numGlxVisuals;
	    }

	    configprivs = xalloc(nconfigs * sizeof(dmxGlxVisualPrivate*));

	    if (configs != NULL && configprivs != NULL) {

		/* Initialize our private info for each visual
		 * (currently only x_visual_depth and x_visual_class)
		 */
		for (i = 0; i < nconfigs; i++) {

		    configprivs[i] = (dmxGlxVisualPrivate *)
			xalloc(sizeof(dmxGlxVisualPrivate));
		    configprivs[i]->x_visual_depth = 0;
		    configprivs[i]->x_visual_class = 0;

		    /* Find the visual depth */
		    if (configs[i].vid > 0) {
			int  j;
			for (j = 0; j < dmxScreen->beNumVisuals; j++) {
			    if (dmxScreen->beVisuals[j].visualid ==
				configs[i].vid) {
				configprivs[i]->x_visual_depth =
				    dmxScreen->beVisuals[j].depth;
				configprivs[i]->x_visual_class =
				    dmxScreen->beVisuals[j].class;
				break;
			    }
			}
		    }
		}

		/* Hand out the glx configs to glx extension */
		GlxSetVisualConfigs(nconfigs, configs, (void**)configprivs);

                XFlush(dmxScreen->beDisplay);
	    }
	}
#endif  /* GLXEXT */

	AddScreen(dmxScreenInit, argc, argv);
    }

    /* Make sure there is a global width/height available */
    dmxComputeWidthHeight ();

    dmxInitProps();
    dmxInitGrabs();
    dmxInitSelections();

#ifdef MITSHM
    dmxInitShm();
#endif

#ifdef RENDER
    /* Initialize the render extension */
    if (!noRenderExtension)
	dmxInitRender();
#endif

#ifdef COMPOSITE
    /* Initialize the composite extension */
    if (!noCompositeExtension)
	dmxInitComposite();
#endif

    /* Initialized things that need timer hooks */
    dmxStatInit();
    dmxSyncInit();              /* Calls RegisterBlockAndWakeupHandlers */

    for (i = 0; i < dmxNumScreens; i++)
	if (dmxScreens[i].beDisplay)
	    dmxBEScreenInit (screenInfo.screens[i]);
}

/* RATS: Assuming the fp string (which comes from the command-line argv
         vector) is NULL-terminated, the buffer is large enough for the
         strcpy. */ 
static void dmxSetDefaultFontPath(char *fp)
{
    int fplen = strlen(fp) + 1;
    
    if (dmxFontPath) {
	int len;

	len = strlen(dmxFontPath);
	dmxFontPath = xrealloc(dmxFontPath, len+fplen+1);
	dmxFontPath[len] = ',';
	strncpy(&dmxFontPath[len+1], fp, fplen);
    } else {
	dmxFontPath = xalloc(fplen);
	strncpy(dmxFontPath, fp, fplen);
    }

    defaultFontPath = dmxFontPath;
}

/** This function is called in Xserver/os/utils.c from \a AbortServer().
 * We must ensure that backend and console state is restored in the
 * event the server shutdown wasn't clean. */
void AbortDDX(void)
{
    int i;

    for (i=0; i < dmxNumScreens; i++) {
        DMXScreenInfo *dmxScreen = &dmxScreens[i];
        
        if (dmxScreen->beDisplay) XCloseDisplay(dmxScreen->beDisplay);
        dmxScreen->beDisplay = NULL;
    }

    dmxAbortDisplay ();
}

/** This function is called in Xserver/dix/main.c from \a main() when
 * dispatchException & DE_TERMINATE (which is the only way to exit the
 * main loop without an interruption. */
void ddxGiveUp(void)
{
    AbortDDX();
}

#ifdef PANORAMIX
static Bool dmxNoPanoramiXExtension = FALSE;
#endif

/** This function is called in Xserver/os/osinit.c from \a OsInit(). */
void OsVendorInit(void)
{
    if (!dmxPropTrans)
    {
	dmxPropTrans = xalloc (sizeof (DMXPropTrans) * 2);
	dmxPropTrans[0].name = "ATOM_PAIR";
	dmxPropTrans[0].format = "aa..";
	dmxPropTrans[0].type = 0;
	dmxPropTrans[1].name = "_COMPIZ_WINDOW_DECOR";
	dmxPropTrans[1].format = "xP";
	dmxPropTrans[1].type = 0;
	dmxPropTransNum = 2;
    }

    if (!dmxSelectionMap)
    {
	dmxSelectionMap = xalloc (sizeof (DMXSelectionMap) * 9);
	dmxSelectionMap[0].name = "WM_S0";
	dmxSelectionMap[0].atom = 0;
	dmxSelectionMap[0].beAtom = 0;
	dmxSelectionMap[1].name = "_NET_WM_CM_S0";
	dmxSelectionMap[1].atom = 0;
	dmxSelectionMap[1].beAtom = 0;
	dmxSelectionMap[2].name = "_NET_SYSTEM_TRAY_S0";
	dmxSelectionMap[2].atom = 0;
	dmxSelectionMap[2].beAtom = 0;
	dmxSelectionMap[3].name = "_NET_DESKTOP_LAYOUT_S0";
	dmxSelectionMap[3].atom = 0;
	dmxSelectionMap[3].beAtom = 0;
	dmxSelectionMap[4].name = "_NET_DESKTOP_MANAGER_S0";
	dmxSelectionMap[4].atom = 0;
	dmxSelectionMap[4].beAtom = 0;
	dmxSelectionMap[5].name = "_XSETTINGS_S0";
	dmxSelectionMap[5].atom = 0;
	dmxSelectionMap[5].beAtom = 0;
	dmxSelectionMap[6].name = "CLIPBOARD_MANAGER";
	dmxSelectionMap[6].atom = 0;
	dmxSelectionMap[6].beAtom = 0;
	dmxSelectionMap[7].name = "GVM_SELECTION";
	dmxSelectionMap[7].atom = 0;
	dmxSelectionMap[7].beAtom = 0;
	dmxSelectionMap[8].name = "_COMPIZ_DM_S0";
	dmxSelectionMap[8].atom = 0;
	dmxSelectionMap[8].beAtom = 0;
	dmxSelectionMapNum = 9;
    }

#ifdef PANORAMIX
    noPanoramiXExtension = dmxNoPanoramiXExtension;
    PanoramiXExtensionDisabledHack = TRUE;
#endif

#ifdef XV
    if (!dmxXvImageFormats)
    {
	dmxXvImageFormats = xalloc (sizeof (char *) * 2);
	dmxXvImageFormats[0] = "YV12";
	dmxXvImageFormats[1] = "YUY2";
	dmxXvImageFormatsNum = 2;
    }
#endif

}

/** This function is called in Xserver/os/utils.c from \a FatalError()
 * and \a VFatalError().  (Note that setting the function pointer \a
 * OsVendorVErrorFProc will cause \a VErrorF() (which is called by the
 * two routines mentioned here, as well as by others) to use the
 * referenced routine instead of \a vfprintf().) */
void OsVendorFatalError(void)
{
}

/** Process our command line arguments. */
int ddxProcessArgument(int argc, char *argv[], int i)
{
    int retval = 0;

    if (!strcmp(argv[i], "-display")) {
	if (++i < argc) dmxAddScreen(argv[i], argv[i], NULL, 0, NULL, 0, 0);
        retval = 2;
    } else if (!strcmp(argv[i], "-numDetached")) {
	if (++i < argc) dmxNumDetached = atoi (argv[i]);
        retval = 2;
    } else if (!strcmp(argv[i], "-fontpath")) {
        if (++i < argc) dmxSetDefaultFontPath(argv[i]);
        retval = 2;
    } else if (!strcmp(argv[i], "-stat")) {
        if ((i += 2) < argc) dmxStatActivate(argv[i-1], argv[i]);
        retval = 3;
    } else if (!strcmp(argv[i], "-syncbatch")) {
        if (++i < argc) dmxSyncActivate(argv[i]);
        retval = 2;
    } else if (!strcmp(argv[i], "-offscreenopt")) {
	dmxOffScreenOpt = TRUE;
        retval = 1;
    } else if (!strcmp(argv[i], "-nosubdivprims")) {
	dmxSubdividePrimitives = FALSE;
        retval = 1;
    } else if (!strcmp(argv[i], "-windowopt")) {
	dmxLazyWindowCreation = TRUE;
        retval = 1;
    } else if (!strcmp(argv[i], "-noxkb")) {
	dmxUseXKB = FALSE;
        retval = 1;
    } else if (!strcmp(argv[i], "-depth")) {
        if (++i < argc) dmxDepth = atoi(argv[i]);
        retval = 2;
    } else if (!strcmp(argv[i], "-norender")) {
	noRenderExtension = TRUE;
        retval = 1;
#ifdef GLXEXT
    } else if (!strcmp(argv[i], "-noglxproxy")) {
	dmxGLXProxy = FALSE;
        retval = 1;
    } else if (!strcmp(argv[i], "-noglxswapgroup")) {
	dmxGLXSwapGroupSupport = FALSE;
        retval = 1;
    } else if (!strcmp(argv[i], "-glxsyncswap")) {
	dmxGLXSyncSwap = TRUE;
        retval = 1;
    } else if (!strcmp(argv[i], "-glxfinishswap")) {
	dmxGLXFinishSwap = TRUE;
        retval = 1;
#endif
    } else if (!strcmp(argv[i], "-ignorebadfontpaths")) {
	dmxIgnoreBadFontPaths = TRUE;
        retval = 1;
    } else if (!strcmp(argv[i], "-noaddremovescreens")) {
	dmxAddRemoveScreens = FALSE;
        retval = 1;
#ifdef PANORAMIX
    } else if (!strcmp (argv[i], "-xinerama")) {
	dmxNoPanoramiXExtension = TRUE;
	retval = 1;
#endif
#ifdef RANDR
    } else if (!strcmp(argv[i], "-outputs")) {
	if (++i < argc) xRROutputsPerScreen = atoi(argv[i]);
        retval = 2;
    } else if (!strcmp(argv[i], "-crtcs")) {
	if (++i < argc) xRRCrtcsPerScreen = atoi(argv[i]);
        retval = 2;
    }
#endif
    else if (!strcmp (argv[i], "-prop"))
    {
	if ((i + 2) < argc)
	{
	    DMXPropTrans *prop;

	    prop = xrealloc (dmxPropTrans, sizeof (DMXPropTrans) *
			     (dmxPropTransNum + 1));
	    if (prop)
	    {
		prop[dmxPropTransNum].name   = argv[i + 1];
		prop[dmxPropTransNum].format = argv[i + 2];
		prop[dmxPropTransNum].type   = 0;

		dmxPropTransNum++;
		dmxPropTrans = prop;
	    }
	}
        retval = 3;
    }
    else if (!strcmp (argv[i], "-selection"))
    {
	if (++i < argc)
	{
	    DMXSelectionMap *selection;

	    selection = xrealloc (dmxSelectionMap, sizeof (DMXSelectionMap) *
				  (dmxSelectionMapNum + 1));
	    if (selection)
	    {
		selection[dmxSelectionMapNum].name   = argv[i];
		selection[dmxSelectionMapNum].atom   = 0;
		selection[dmxSelectionMapNum].beAtom = 0;

		dmxSelectionMapNum++;
		dmxSelectionMap = selection;
	    }
	}
        retval = 2;
    }
#ifdef XV
    else if (!strcmp (argv[i], "-xvimage"))
    {
	if (++i < argc)
	{
	    char **formats;

	    formats = xrealloc (dmxXvImageFormats,
				sizeof (char *) *
				(dmxXvImageFormatsNum + 1));
	    if (formats)
	    {
		formats[dmxXvImageFormatsNum] = argv[i];

		dmxXvImageFormatsNum++;
		dmxXvImageFormats = formats;
	    }
	}
        retval = 2;
    }
#endif
    else if ((argv[i][0] == 'v') && (argv[i][1] == 't'))
    {
        dmxLaunchVT = argv[i];
	retval = 1;
    }
    else if (!strcmp(argv[i], "--")) {
        dmxLaunchIndex = i + 1;
        retval = argc - i;
    }

    return retval;
}

/** Provide succinct usage information for the DMX server. */
void ddxUseMsg(void)
{
    ErrorF("\n\nDevice Dependent Usage:\n");
    ErrorF("-display string      Specify the back-end display(s)\n");
    ErrorF("-numDetached num     Specify detached back-end display(s)\n");
    ErrorF("-shadowfb            Enable shadow frame buffer\n");
    ErrorF("-fontpath            Sets the default font path\n");
    ErrorF("-stat inter scrns    Print out performance statistics\n");
    ErrorF("-syncbatch inter     Set interval for XSync batching\n");
    ErrorF("-offscreenopt        Enable offscreen optimization\n");
    ErrorF("-nosubdivprims       Disable primitive subdivision\n");
    ErrorF("                     optimization\n");
    ErrorF("-windowopt           Enable lazy window creation optimization\n");
    ErrorF("-noxkb               Disable use of the XKB extension with\n");
    ErrorF("                     backend displays (cf. -kb).\n");
    ErrorF("-depth               Specify the default root window depth\n");
    ErrorF("-norender            Disable RENDER extension support\n");
#ifdef GLXEXT
    ErrorF("-glxproxy            Enable GLX Proxy\n");
    ErrorF("-noglxswapgroup      Disable swap group and swap barrier\n");
    ErrorF("                     extensions in GLX proxy\n");
    ErrorF("-glxsyncswap         Force XSync after swap buffers\n");
    ErrorF("-glxfinishswap       Force glFinish after swap buffers\n");
#endif
    ErrorF("-ignorebadfontpaths  Ignore bad font paths during initialization\n");
    ErrorF("-noaddremovescreens  Disable dynamic screen addition/removal\n");
#ifdef RANDR
    ErrorF("-outputs num         RANDR outputs for each back-end display\n");
    ErrorF("-crtcs num           RANDR crtcs for each back-end display\n");
#endif
    ErrorF("-prop name format    Specify property translation\n");
    ErrorF("-selection name      Specify selection that needs unique prefix\n");
#ifdef XV
    ErrorF("-xvimage fourcc      Enable XVideo image format\n");
#endif
    ErrorF("--  [ server ] [ display ] [ options ]\n");
}
