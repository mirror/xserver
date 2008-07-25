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

#ifdef XV
#include "dmx.h"
#include "dmxsync.h"
#include "dmxgc.h"
#include "dmxwindow.h"
#include "dmxvisual.h"
#include "dmxlog.h"
#include "dmxxv.h"

#include "xvdix.h"
#include "gcstruct.h"
#include "dixstruct.h"
#include "servermd.h"
#include "pixmapstr.h"

typedef struct _dmxXvAdaptor {
    int base;
    int n;
} dmxXvAdaptorRec, *dmxXvAdaptorPtr;

typedef struct _dmxXvPort {
    int      id;
    XvPortID port;
    XvImage  *image;
} dmxXvPortRec, *dmxXvPortPtr;

#define DMX_GET_XV_SCREEN(pScreen)			    \
    ((XvScreenPtr) dixLookupPrivate (&pScreen->devPrivates, \
				     XvGetScreenKey ()))

#define DMX_XV_SCREEN(pScreen)				\
    XvScreenPtr pXvScreen = DMX_GET_XV_SCREEN (pScreen)

#define DMX_GET_XV_ADAPTOR_PRIV(pAdaptor)		\
    ((dmxXvAdaptorPtr) ((pAdaptor)->devPriv.ptr))

#define DMX_XV_ADAPTOR_PRIV(pAdaptor)					\
    dmxXvAdaptorPtr pAdaptorPriv = DMX_GET_XV_ADAPTOR_PRIV (pAdaptor)

#define DMX_GET_XV_PORT_PRIV(pPort)		\
    ((dmxXvPortPtr) ((pPort)->devPriv.ptr))

#define DMX_XV_PORT_PRIV(pPort)				  \
    dmxXvPortPtr pPortPriv = DMX_GET_XV_PORT_PRIV (pPort)

#define DMX_XV_NUM_PORTS 32

#define DMX_XV_IMAGE_MAX_WIDTH  2046
#define DMX_XV_IMAGE_MAX_HEIGHT 2046

#define DMX_FOURCC(a, b, c, d)					\
    ((a) | (b) << 8 | (c) << 16 | ((unsigned int) (d)) << 24)

#define DMX_FOURCC_YUY2 DMX_FOURCC ('Y', 'U', 'Y', '2')
#define DMX_FOURCC_YV12 DMX_FOURCC ('Y', 'V', '1', '2')

static XvImageRec xvImages[] = {
    {
	DMX_FOURCC_YUY2, XvYUV, BITMAP_BIT_ORDER,
	{
	    'Y','U','Y','2',
	    0x00, 0x00, 0x00, 0x10, 0x80, 0x00,
	    0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
	},
	16, XvPacked, 1,
	0, 0, 0, 0,
	8, 8, 8,  1, 2, 2,  1, 1, 1,
	{
	    'Y', 'U', 'Y', 'V',
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	XvTopToBottom
    }, {
	DMX_FOURCC_YV12, XvYUV, BITMAP_BIT_ORDER,
	{
	    'Y', 'V', '1', '2',
	    0x00, 0x00, 0x00, 0x10, 0x80, 0x00,
	    0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
	},
	12, XvPlanar, 3,
	0, 0, 0, 0,
	8, 8, 8,  1, 2, 2,  1, 2, 2,
	{
	    'Y', 'V', 'U', 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	XvTopToBottom
    }
};

static int
dmxXvFreePort (XvPortPtr pPort)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pPort->pAdaptor->pScreen->myNum];

    DMX_XV_PORT_PRIV (pPort);

    if (pPortPriv->port)
    {
	XLIB_PROLOGUE (dmxScreen);
	XvUngrabPort (dmxScreen->beDisplay, pPortPriv->port, CurrentTime);
	XLIB_EPILOGUE (dmxScreen);

	pPortPriv->port = 0;
    }

    if (pPortPriv->image)
    {
	XFree (pPortPriv->image);
	pPortPriv->image = NULL;
    }

    pPortPriv->id = 0;

    return Success;
}

static int
dmxXvStopVideo (ClientPtr   client,
		XvPortPtr   pPort,
		DrawablePtr pDrawable)
{
    return Success;
}

static int
dmxSetPortAttribute (ClientPtr client,
		     XvPortPtr pPort,
		     Atom      atom,
		     INT32     value)
{
    return BadMatch;
}

static int
dmxGetPortAttribute (ClientPtr client,
		     XvPortPtr pPort,
		     Atom      atom,
		     INT32     *value)
{
    return BadMatch;
}

static int
dmxXvQueryBestSize (ClientPtr	 client,
		    XvPortPtr	 pPort,
		    CARD8	 motion,
		    CARD16	 srcWidth,
		    CARD16	 srcHeight,
		    CARD16	 dstWidth,
		    CARD16	 dstHeight,
		    unsigned int *pWidth,
		    unsigned int *pHeight)
{
    *pWidth  = dstWidth;
    *pHeight = dstHeight;

    return Success;
}

static int
dmxXvPutImage (ClientPtr     client,
	       DrawablePtr   pDrawable,
	       XvPortPtr     pPort,
	       GCPtr	     pGC,
	       INT16	     srcX,
	       INT16	     srcY,
	       CARD16	     srcWidth,
	       CARD16	     srcHeight,
	       INT16	     dstX,
	       INT16	     dstY,
	       CARD16	     dstWidth,
	       CARD16	     dstHeight,
	       XvImagePtr    pImage,
	       unsigned char *data,
	       Bool	     sync,
	       CARD16	     width,
	       CARD16	     height)
{
    ScreenPtr     pScreen = pDrawable->pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV ((WindowPtr) pDrawable);
    dmxGCPrivPtr  pGCPriv = DMX_GET_GC_PRIV (pGC);

    DMX_XV_PORT_PRIV (pPort);

    if (pPortPriv->id != pImage->id)
    {
	int p, j;

	DMX_XV_ADAPTOR_PRIV (pPort->pAdaptor);

	for (p = pAdaptorPriv->base;
	     p < pAdaptorPriv->base + pAdaptorPriv->n;
	     p++)
	{
	    XvImageFormatValues	*fo = NULL;
	    int                 formats;

	    XLIB_PROLOGUE (dmxScreen);
	    fo = XvListImageFormats (dmxScreen->beDisplay, p, &formats);
	    XLIB_EPILOGUE (dmxScreen);

	    if (fo)
	    {
		for (j = 0; j < formats; j++)
		    if (fo[j].id == pImage->id)
			break;

		XFree (fo);

		if (j < formats)
		{
		    if (p != pPortPriv->port)
		    {
			int ret = !Success;

			XLIB_PROLOGUE (dmxScreen);
			ret = XvGrabPort (dmxScreen->beDisplay,
					  p,
					  CurrentTime);
			XLIB_EPILOGUE (dmxScreen);

			if (ret == Success)
			    break;
		    }
		    else
			break;
		}

	    }
	}

	pPortPriv->id = pImage->id;

	if (pPortPriv->image)
	{
	    XFree (pPortPriv->image);
	    pPortPriv->image = NULL;
	}

	if (pPortPriv->port && p != pPortPriv->port)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    XvUngrabPort (dmxScreen->beDisplay, pPortPriv->port, CurrentTime);
	    XLIB_EPILOGUE (dmxScreen);

	    pPortPriv->port = 0;
	}

	if (p < pAdaptorPriv->base + pAdaptorPriv->n)
	{
	    pPortPriv->port = p;
	}
	else if (pAdaptorPriv->n)
	{
	    dmxLog (dmxWarning,
		    "XVIDEO: failed to allocated port "
		    "for image format: 0x%x\n", pImage->id);
	}
    }
    else if (pPortPriv->image)
    {
	if (width  != pPortPriv->image->width ||
	    height != pPortPriv->image->height)
	{
	    XFree (pPortPriv->image);
	    pPortPriv->image = NULL;
	}
    }

    if (!pPortPriv->image && pPortPriv->port)
    {
	XLIB_PROLOGUE (dmxScreen);
	pPortPriv->image = XvCreateImage (dmxScreen->beDisplay,
					  pPortPriv->port,
					  pImage->id,
					  NULL,
					  width, 
					  height);
	XLIB_EPILOGUE (dmxScreen);

	if (!pPortPriv->image)
	    return BadAlloc;

	dmxLogOutput (dmxScreen,
		      "XVIDEO: created 0x%x image with dimensions %dx%d\n",
		      pImage->id, width, height);
    }

    if (pPortPriv->image)
    {
	pPortPriv->image->data = (char *) data;

	XLIB_PROLOGUE (dmxScreen);
	XvPutImage (dmxScreen->beDisplay,
		    pPortPriv->port,
		    pWinPriv->window,
		    pGCPriv->gc,
		    pPortPriv->image,
		    srcX, srcY, srcWidth, srcHeight,
		    dstX, dstY, dstWidth, dstHeight);
	XLIB_EPILOGUE (dmxScreen);

	dmxSync (dmxScreen, FALSE);
    }

    return Success;
}

static int
dmxXvQueryImageAttributes (ClientPtr  client,
			   XvPortPtr  pPort,
			   XvImagePtr pImage,
			   CARD16     *width,
			   CARD16     *height,
			   int	      *pitches,
			   int	      *offsets)
{
    if (*width > DMX_XV_IMAGE_MAX_WIDTH)
	*width = DMX_XV_IMAGE_MAX_WIDTH;

    if (*height > DMX_XV_IMAGE_MAX_HEIGHT)
	*height = DMX_XV_IMAGE_MAX_HEIGHT;

    *width = (*width + 7) & ~7;

    switch (pImage->id) {
    case DMX_FOURCC_YUY2:
	if (offsets)
	    offsets[0] = 0;

	if (pitches)
	    pitches[0] = *width * 2;

	return *width * *height * 2;
    case DMX_FOURCC_YV12:
	*height = (*height + 1) & ~1;

	if (offsets)
	{
	    offsets[0] = 0;
	    offsets[1] = *width * *height;
	    offsets[2] = *width * *height + (*width >> 1) * (*height >> 1);
	}

	if (pitches)
	{
	    pitches[0] = *width;
	    pitches[1] = pitches[2] = *width >> 1;
	}

	return *width * *height + (*width >> 1) * *height;
    default:
	return 0;
    }
}

static void
dmxXvFreeAdaptor (XvAdaptorPtr pAdaptor)
{
    xfree (pAdaptor->pEncodings);
    xfree (pAdaptor->pFormats);

    if (pAdaptor->pPorts)
	xfree (pAdaptor->pPorts);
}

static Bool
dmxXvInitAdaptors (ScreenPtr pScreen)
{
    dmxXvAdaptorPtr pAdaptorPriv;
    XvAdaptorPtr    pAdaptor;
    dmxXvPortPtr    pPortPriv;
    XvPortPtr       pPort;
    XvFormatPtr     pFormat;
    XvEncodingPtr   pEncoding;
    XvImagePtr      pImages;
    int		    i, j, nImages = 0;
    DMXScreenInfo   *dmxScreen = &dmxScreens[pScreen->myNum];

    DMX_XV_SCREEN (pScreen);

    pXvScreen->nAdaptors = 0;
    pXvScreen->pAdaptors = NULL;

    pImages = xalloc (sizeof (xvImages));
    if (!pImages)
	return FALSE;

    for (i = 0; i < dmxXvImageFormatsNum; i++)
    {
	for (j = 0; j < sizeof (xvImages) / sizeof (XvImageRec); j++)
	{
	    char imageName[5];

	    sprintf (imageName, "%c%c%c%c",
		     xvImages[j].id & 0xff,
		     (xvImages[j].id >> 8) & 0xff,
		     (xvImages[j].id >> 16) & 0xff,
		     (xvImages[j].id >> 24) & 0xff);

	    if (strcmp (imageName, dmxXvImageFormats[i]) == 0 ||
		strtol (dmxXvImageFormats[i], NULL, 0) == xvImages[j].id)
	    {
		dmxLogOutput (dmxScreen, "XVIDEO: using image format: \n");
		dmxLogOutput (dmxScreen,"      id: 0x%x", xvImages[j].id);
		if (isprint (imageName[0]) && isprint (imageName[1]) &&
		    isprint (imageName[2]) && isprint (imageName[3])) 
		    dmxLogOutputCont (dmxScreen, " (%s)\n", imageName);
		else
		    dmxLogOutputCont (dmxScreen, "\n", imageName);

		dmxLogOutput (dmxScreen, "        bits per pixel: %i\n",
			      xvImages[j].bits_per_pixel);
		dmxLogOutput (dmxScreen, "        number of planes: %i\n",
			      xvImages[j].num_planes);

		dmxLogOutput (dmxScreen, "        type: %s (%s)\n",
			      (xvImages[j].type == XvRGB) ? "RGB" : "YUV",
			      (xvImages[j].format == XvPacked) ?
			      "packed" : "planar");

		if (xvImages[j].type == XvRGB)
		{
		    dmxLogOutput (dmxScreen, "        depth: %i\n",
				  xvImages[j].depth);
		    dmxLogOutput (dmxScreen,
				  "        red, green, blue masks: "
				  "0x%x, 0x%x, 0x%x\n",
				  xvImages[j].red_mask,
				  xvImages[j].green_mask,
				  xvImages[j].blue_mask);
		}

		pImages[nImages++] = xvImages[j];
		break;
	    }
	}

	if (j == sizeof (xvImages) / sizeof (XvImageRec))
	{
	    dmxLogOutput (dmxScreen,
			  "XVIDEO: unsupported image format %s\n",
			  dmxXvImageFormats[i]);
	}
    }

    if (!nImages)
    {
	dmxLogOutput (dmxScreen, "XVIDEO: no supported image formats "
		      "enabled\n");
	xfree (pImages);
	return TRUE;
    }

    pAdaptor = Xcalloc (1, sizeof (XvAdaptorRec) + sizeof (dmxXvAdaptorRec));
    pAdaptorPriv = (dmxXvAdaptorPtr) (pAdaptor + 1);
    if (!pAdaptor)
	return FALSE;

    pAdaptor->type    = XvInputMask | XvImageMask;
    pAdaptor->pScreen = pScreen;

    pAdaptor->ddFreePort	     = dmxXvFreePort;
    pAdaptor->ddStopVideo	     = dmxXvStopVideo;
    pAdaptor->ddSetPortAttribute     = dmxSetPortAttribute;
    pAdaptor->ddGetPortAttribute     = dmxGetPortAttribute;
    pAdaptor->ddPutImage	     = dmxXvPutImage;
    pAdaptor->ddQueryBestSize	     = dmxXvQueryBestSize;
    pAdaptor->ddQueryImageAttributes = dmxXvQueryImageAttributes;

    pAdaptor->name = "DMX Video";

    pEncoding = Xcalloc (1, sizeof (XvEncodingRec));
    if (!pEncoding)
	return FALSE;

    pEncoding->id      = 0;
    pEncoding->pScreen = pScreen;
    pEncoding->name    = "XV_IMAGE";

    pEncoding->width  = DMX_XV_IMAGE_MAX_WIDTH;
    pEncoding->height = DMX_XV_IMAGE_MAX_HEIGHT;

    pEncoding->rate.numerator	= 1;
    pEncoding->rate.denominator = 1;

    pAdaptor->nEncodings = 1;
    pAdaptor->pEncodings = pEncoding;

    pAdaptor->nImages = nImages;
    pAdaptor->pImages = pImages;

    pAdaptor->nAttributes = 0;
    pAdaptor->pAttributes = 0;

    pFormat = Xcalloc (1, sizeof (XvFormatRec));
    if (!pFormat)
	return FALSE;

    pFormat->depth  = pScreen->rootDepth;
    pFormat->visual = pScreen->rootVisual;

    pAdaptor->nFormats = 1;
    pAdaptor->pFormats = pFormat;

    pPort = Xcalloc (DMX_XV_NUM_PORTS,
		     sizeof (XvPortRec) + sizeof (dmxXvPortRec));
    pPortPriv = (dmxXvPortPtr) (pPort + DMX_XV_NUM_PORTS);
    if (!pPort)
	return FALSE;

    for (i = 0; i < DMX_XV_NUM_PORTS; i++)
    {
	pPort[i].id = FakeClientID (0);

	if (!AddResource (pPort[i].id, XvGetRTPort (), &pPort[i]))
	    return FALSE;

	pPort[i].pAdaptor    = pAdaptor;
	pPort[i].pNotify     = (XvPortNotifyPtr) 0;
	pPort[i].pDraw	     = (DrawablePtr) 0;
	pPort[i].client      = (ClientPtr) 0;
	pPort[i].grab.client = (ClientPtr) 0;
	pPort[i].time	     = currentTime;
	pPort[i].devPriv.ptr = pPortPriv + i;
    }

    pAdaptor->nPorts      = DMX_XV_NUM_PORTS;
    pAdaptor->pPorts      = pPort;
    pAdaptor->base_id     = pPort->id;
    pAdaptor->devPriv.ptr = pAdaptorPriv;

    pXvScreen->pAdaptors = pAdaptor;
    pXvScreen->nAdaptors = 1;

    return TRUE;
}

static Bool
dmxXvCloseScreen (int i, ScreenPtr pScreen)
{
    int	j;

    DMX_XV_SCREEN (pScreen);

    for (j = 0; j < pXvScreen->nAdaptors; j++)
	dmxXvFreeAdaptor (&pXvScreen->pAdaptors[j]);

    if (pXvScreen->pAdaptors)
	xfree (pXvScreen->pAdaptors);

    return TRUE;
}

static int
dmxXvQueryAdaptors (ScreenPtr	 pScreen,
		    XvAdaptorPtr *pAdaptors,
		    int		 *nAdaptors)
{
    DMX_XV_SCREEN (pScreen);

    *nAdaptors = pXvScreen->nAdaptors;
    *pAdaptors = pXvScreen->pAdaptors;

    return Success;
}

void
dmxBEXvScreenInit (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    XvAdaptorInfo *ai;
    unsigned int  nAdaptors;
    int           i, j, k, l, ret = !Success;

    DMX_XV_SCREEN (pScreen);

    XLIB_PROLOGUE (dmxScreen);
    ret = XvQueryAdaptors (dmxScreen->beDisplay,
			   DefaultRootWindow (dmxScreen->beDisplay),
			   &nAdaptors,
			   &ai);
    XLIB_EPILOGUE (dmxScreen);

    if (ret != Success)
    {
	dmxLogOutput (dmxScreen,
		      "XVIDEO: back-end support unavailable. "
		      "XvQueryAdaptors returned %d\n",
		      ret);
	return;
    }

    for (i = 0; i < pXvScreen->nAdaptors; i++)
    {
	XvAdaptorPtr pAdaptor = &pXvScreen->pAdaptors[i];

	DMX_XV_ADAPTOR_PRIV (pAdaptor);

	for (j = 0; j < nAdaptors; j++)
	{
	    if ((pAdaptor->type & ai[j].type) != pAdaptor->type)
		continue;

	    for (k = 0; k < pAdaptor->nFormats; k++)
	    {
		Visual   *visual;
		VisualID vid;
		int      depth = pAdaptor->pFormats[k].depth;

		visual = dmxLookupVisualFromID (pScreen,
						pAdaptor->pFormats[k].visual);
		if (visual)
		    vid = XVisualIDFromVisual (visual);
		else
		    vid = 0;
		    
		for (l = 0; l < ai[j].num_formats; l++)
		{
		    if (ai[j].formats[l].depth     == depth &&
			ai[j].formats[l].visual_id == vid)
			break;
		}

		if (l == ai[j].num_formats)
		    break;
	    }

	    if (k < pAdaptor->nFormats)
		continue;

	    dmxLogOutput (dmxScreen,
			  "XVIDEO: Using back-end adaptor:\n"
			  "  name:        %s\n"
			  "  type:        %s%s%s%s%s\n"
			  "  ports:       %ld\n"
			  "  first port:  %ld\n",
			  ai[j].name,
			  (ai[j].type & XvInputMask)	? "input | "	: "",
			  (ai[j].type & XvOutputMask)	? "output | "	: "",
			  (ai[j].type & XvVideoMask)	? "video | "	: "",
			  (ai[j].type & XvStillMask)	? "still | "	: "",
			  (ai[j].type & XvImageMask)	? "image"	: "",
			  ai[j].num_ports,
			  ai[j].base_id);

	    pAdaptorPriv->base = ai[j].base_id;
	    pAdaptorPriv->n    = ai[j].num_ports;
	    break;
	}

	if (j == nAdaptors)
	    dmxLogOutput (dmxScreen, "XVIDEO: No usable back-end adaptors "
			  "found for '%s'\n", pAdaptor->name);
    }

    if (nAdaptors > 0)
	XvFreeAdaptorInfo (ai);
}

void
dmxBEXvScreenFini (ScreenPtr pScreen)
{
    int i, j;

    DMX_XV_SCREEN (pScreen);

    for (i = 0; i < pXvScreen->nAdaptors; i++)
    {
	DMX_XV_ADAPTOR_PRIV (&pXvScreen->pAdaptors[i]);

	for (j = 0; j < pXvScreen->pAdaptors[i].nPorts; j++)
	    dmxXvFreePort (&pXvScreen->pAdaptors[i].pPorts[j]);

	pAdaptorPriv->base = 0;
	pAdaptorPriv->n    = 0;
    }
}

Bool
dmxXvScreenInit (ScreenPtr pScreen)
{
    XvScreenPtr pXvScreen;
    int		status;

    status = XvScreenInit (pScreen);
    if (status != Success)
	return FALSE;

    pXvScreen = DMX_GET_XV_SCREEN (pScreen);
    pXvScreen->ddCloseScreen   = dmxXvCloseScreen;
    pXvScreen->ddQueryAdaptors = dmxXvQueryAdaptors;
    pXvScreen->devPriv.ptr     = (pointer) 0;

    if (!dmxXvInitAdaptors (pScreen))
	return FALSE;

    return TRUE;
}

#endif
