/*
 * Copyright 2006 Zack Rusin
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * ZACK RUSIN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ZACK RUSIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Alan Hourihane <alanh@tungstengraphics.com>
 *
 * Re-written from original code by Zack Rusin
 * 
 **************************************************************************/

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include "glxserver.h"
#include "glucose.h"
#include "glitz_glucose.h"

#include "xgl.h"

#include "xf86str.h"
#include "xf86.h"

#ifdef MITSHM
#include "shmint.h"
static ShmFuncs shmFuncs = { NULL, xglShmPutImage };
#endif

extern __GLXscreen **__glXActiveScreens;
int xglScreenGeneration;
int xglScreenPrivateIndex;
int xglGCPrivateIndex;
int xglPixmapPrivateIndex;
int xglWinPrivateIndex;
#ifdef RENDER
int xglGlyphPrivateIndex;
#endif

static int glucoseGeneration = -1;
int glucoseScreenPrivateIndex;
int glucoseCreateScreenResourcesIndex;

xglScreenInfoRec xglScreenInfo = {
    NULL, 0, 0, 0, 0, 0,
    DEFAULT_GEOMETRY_DATA_TYPE,
    DEFAULT_GEOMETRY_USAGE,
    FALSE,
    XGL_DEFAULT_PBO_MASK,
    FALSE,
    FALSE,
    FilterBilinear,
    {
	{ FALSE, FALSE, { 0, 0, 0, 0 } },
	{ FALSE, FALSE, { 0, 0, 0, 0 } },
	{ FALSE, FALSE, { 0, 0, 0, 0 } },
	{ FALSE, FALSE, { 0, 0, 0, 0 } }
    }
};

static glitz_drawable_format_t *
glucoseInitOutput(__GLXscreen *screen);

static Bool
glucoseCreateScreenResources(ScreenPtr pScreen)
{
  int ret = TRUE;
  CreateScreenResourcesProcPtr CreateScreenResources =
    (CreateScreenResourcesProcPtr)(pScreen->devPrivates[glucoseCreateScreenResourcesIndex].ptr);
  GlucoseScreenPrivPtr pScreenPriv = GlucoseGetScreenPriv(pScreen);
  int err;

  xf86DrvMsg(pScreen->myNum, X_INFO, 
		  "Glucose initializing screen %d\n",pScreen->myNum);

  if ( pScreen->CreateScreenResources != glucoseCreateScreenResources ) {
    /* Can't find hook we are hung on */
	xf86DrvMsg(pScreen->myNum, X_WARNING /* X_ERROR */,
		  "glucoseCreateScreenResources %p called when not in pScreen->CreateScreenResources %p n",
		   (void *)glucoseCreateScreenResources,
		   (void *)pScreen->CreateScreenResources );
  }

  /* Unhook this function ... */
  pScreen->CreateScreenResources = CreateScreenResources;
  pScreen->devPrivates[glucoseCreateScreenResourcesIndex].ptr = NULL;

  /* ... and call the previous CreateScreenResources fuction, if any */
  if (NULL!=pScreen->CreateScreenResources) {
    ret = (*pScreen->CreateScreenResources)(pScreen);
  }

  xglScreenInfo.width  = pScreen->width;
  xglScreenInfo.height = pScreen->height;
  xglScreenInfo.widthMm  = pScreen->mmWidth;
  xglScreenInfo.heightMm = pScreen->mmHeight;

  pScreenPriv->screen = __glXActiveScreens[pScreen->myNum];

  {
    glitz_drawable_t	    *drawable;
    glitz_drawable_format_t *format;
    __GLcontextModes *modes = pScreenPriv->screen->modes;
    PixmapPtr pPixmap = pScreen->GetScreenPixmap(pScreen);
    xglScreenPtr xglScreenPriv = XGL_GET_SCREEN_PRIV (pScreen);

    /* track root pixmap */
    if (pPixmap)
    {
	pPixmap->drawable.serialNumber = NEXT_SERIAL_NUMBER;
	pPixmap->drawable.id = FakeClientID(0);
	AddResource(pPixmap->drawable.id, RT_PIXMAP, (pointer)pPixmap);
    }

    pScreenPriv->rootDrawable = pScreenPriv->screen->createDrawable(pScreenPriv->screen, (DrawablePtr)pPixmap, pPixmap->drawable.id, modes);

    if (!pScreenPriv->rootDrawable)
    	return FALSE;

    pScreenPriv->rootContext = pScreenPriv->screen->createContext(pScreenPriv->screen, modes, NULL);

    if (!pScreenPriv->rootContext)
    	return FALSE;

    pScreenPriv->rootContext->drawPriv = 
    	pScreenPriv->rootContext->readPriv = pScreenPriv->rootDrawable;

    __glXleaveServer();
    err = pScreenPriv->rootContext->makeCurrent(pScreenPriv->rootContext);
    if (!err)
	    ErrorF("makecurrent failed, err is %d\n",err);

    format = glucoseInitOutput(pScreenPriv->screen);

    drawable = glitz_glucose_create_drawable_for_window(pScreenPriv->screen,
                                                    format, pScreenPriv->rootDrawable,
                                                    pPixmap->drawable.width,
                                                    pPixmap->drawable.height);

    if (!drawable) {
        xf86DrvMsg(pScreen->myNum, X_ERROR,
		  "Glucose could not create glitz drawable, not initializing.\n");

	pScreenPriv->rootContext->destroy(pScreenPriv->rootContext);
	pScreenPriv->rootContext = NULL;
	pScreenPriv->rootDrawable->destroy(pScreenPriv->rootDrawable);
	pScreenPriv->rootDrawable = NULL;
    	__glXenterServer();
	return FALSE;
    }

    xglScreenInfo.drawable = xglScreenPriv->drawable = drawable;
    xglScreenPriv->features =
	glitz_drawable_get_features (xglScreenInfo.drawable);

    xf86DrvMsg(pScreen->myNum, X_INFO,
		  "Glucose reports GLitz features as 0x%lx\n",xglScreenPriv->features);

    glucoseFinishScreenInit(pScreen);
    __glXenterServer();

    /* now fixup root pixmap */
    pPixmap = pScreen->GetScreenPixmap(pScreen);
    xglPixmapPtr pPixmapPriv = XGL_GET_PIXMAP_PRIV (pPixmap);

    xglPixmapSurfaceInit(pPixmap, xglScreenPriv->features, 0, 0);

    REGION_UNINIT (pPixmap->drawable.pScreen, &pPixmapPriv->bitRegion);

    xglPixmapSurfaceInit(pPixmap, xglScreenPriv->features, pPixmap->drawable.width, pPixmap->drawable.height);

    if (pScreen->devPrivate && pPixmapPriv->pDamage) {
	RegionPtr pRegion = DamageRegion (pPixmapPriv->pDamage);

	REGION_UNINIT (pPixmap->drawable.pScreen, pRegion);
	REGION_INIT (pPixmap->drawable.pScreen, pRegion, NullBox, 0);
	REGION_SUBTRACT (pPixmap->drawable.pScreen, pRegion,
			     &pPixmapPriv->bitRegion, pRegion);

    }

    pPixmapPriv->pVisual = xglScreenPriv->rootVisual;

    pPixmapPriv->target  = xglPixmapTargetIn;

    xglScreenPriv->pScreenPixmap = pPixmap;

    glitz_drawable_reference (xglScreenPriv->drawable);
    pPixmapPriv->drawable = xglScreenPriv->drawable;

    glitz_surface_reference (xglScreenPriv->surface);
    pPixmapPriv->surface = xglScreenPriv->surface;
  }

  return (ret);
}

static Bool
glucoseAllocatePrivates(ScreenPtr pScreen)
{
    GlucoseScreenPrivPtr pScreenPriv;

    if (glucoseGeneration != serverGeneration) {
	glucoseScreenPrivateIndex = AllocateScreenPrivateIndex();
	if (glucoseScreenPrivateIndex < 0)
	    return FALSE;
	glucoseCreateScreenResourcesIndex = AllocateScreenPrivateIndex();
	if (glucoseCreateScreenResourcesIndex < 0)
	    return FALSE;

	glucoseGeneration = serverGeneration;
    }

    pScreenPriv = xalloc(sizeof(GlucoseScreenPrivRec));
    if (!pScreenPriv) {
        LogMessage(X_WARNING, "Glucose(%d): Failed to allocate screen private\n",
		   pScreen->myNum);
	return FALSE;
    }

    pScreen->devPrivates[glucoseScreenPrivateIndex].ptr = (pointer) pScreenPriv;

    pScreen->devPrivates[glucoseCreateScreenResourcesIndex].ptr
	= (void*)(pScreen->CreateScreenResources);
    pScreen->CreateScreenResources = glucoseCreateScreenResources;

    return TRUE;
}

static glitz_drawable_format_t *
glucoseInitOutput(__GLXscreen *screen)
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

    templ.samples          = 1;
    templ.doublebuffer     = 1;
    templ.color.fourcc     = GLITZ_FOURCC_RGB;
    templ.color.alpha_size = 8;

    mask = GLITZ_FORMAT_SAMPLES_MASK | GLITZ_FORMAT_FOURCC_MASK;

    for (i = 0; i < sizeof(extraMask) / sizeof(extraMask[0]); i++)
    {
	format = glitz_glucose_find_window_format(screen,
                                              mask | extraMask[i],
                                              &templ, 0);
	if (format)
	    break;
    }

    if (!format)
	FatalError("no visual format found");

    xglScreenInfo.depth =
	format->color.red_size   +
	format->color.green_size +
	format->color.blue_size;

    return format;
}

/* Here to mimick the xgl counterpart */
static Bool
xglAllocatePrivates (ScreenPtr pScreen)
{
    xglScreenPtr pScreenPriv;

    if (xglScreenGeneration != serverGeneration)
    {
	xglScreenPrivateIndex = AllocateScreenPrivateIndex ();
	if (xglScreenPrivateIndex < 0)
	    return FALSE;

	xglGCPrivateIndex = AllocateGCPrivateIndex ();
	if (xglGCPrivateIndex < 0)
	    return FALSE;

	xglPixmapPrivateIndex = AllocatePixmapPrivateIndex ();
	if (xglPixmapPrivateIndex < 0)
	    return FALSE;

	xglWinPrivateIndex = AllocateWindowPrivateIndex ();
	if (xglWinPrivateIndex < 0)
	    return FALSE;

#ifdef RENDER
	xglGlyphPrivateIndex = AllocateGlyphPrivateIndex ();
	if (xglGlyphPrivateIndex < 0)
	    return FALSE;
#endif

	xglScreenGeneration = serverGeneration;
    }

    if (!AllocateGCPrivate (pScreen, xglGCPrivateIndex, sizeof (xglGCRec)))
	return FALSE;

    if (!AllocatePixmapPrivate (pScreen, xglPixmapPrivateIndex,
				sizeof (xglPixmapRec)))
	return FALSE;

    if (!AllocateWindowPrivate (pScreen, xglWinPrivateIndex,
				sizeof (xglWinRec)))
	return FALSE;

    pScreenPriv = xalloc (sizeof (xglScreenRec));
    if (!pScreenPriv)
	return FALSE;
    memset(pScreenPriv, 0, sizeof(xglScreenRec));

    XGL_SET_SCREEN_PRIV (pScreen, pScreenPriv);

    return TRUE;
}

Bool
glucoseScreenInit (ScreenPtr pScreen, int flags)
{
    xglScreenPtr pScreenPriv;

#ifdef RENDER
    PictureScreenPtr pPictureScreen;
#endif

    if (!glucoseAllocatePrivates(pScreen))
        return FALSE;

    if (!xglAllocatePrivates (pScreen))
	return FALSE;

    pScreenPriv = XGL_GET_SCREEN_PRIV (pScreen);

    pScreenPriv->pScreenPixmap = NULL;

    pScreenPriv->pVisual = 0;

    pScreenPriv->rootVisual = 0;

    GEOMETRY_INIT (pScreen, &pScreenPriv->scratchGeometry,
		   GLITZ_GEOMETRY_TYPE_VERTEX,
		   pScreenPriv->geometryUsage, 0);

    pScreenPriv->geometryDataType = xglScreenInfo.geometryDataType;
    pScreenPriv->geometryUsage    = xglScreenInfo.geometryUsage;
    pScreenPriv->yInverted	  = xglScreenInfo.yInverted;
    pScreenPriv->pboMask	  = xglScreenInfo.pboMask;
    pScreenPriv->lines		  = xglScreenInfo.lines;
    pScreenPriv->noYuv		  = xglScreenInfo.noYuv;
    pScreenPriv->xvFilter	  = xglScreenInfo.xvFilter;
    pScreenPriv->accel		  = xglScreenInfo.accel;

#if 0
    /* add some flags to change the default xgl methods above */
    if (flags & GLUCOSE_xxx) {

    }
#endif


    pScreen->CreatePixmap  = xglCreatePixmap;
    pScreen->DestroyPixmap = xglDestroyPixmap;

#ifdef MITSHM
    ShmRegisterFuncs (pScreen, &shmFuncs);
#endif

    XGL_SCREEN_WRAP (GetImage, xglGetImage);
    XGL_SCREEN_WRAP (GetSpans, xglGetSpans);

    XGL_SCREEN_WRAP (CopyWindow, xglCopyWindow);
    XGL_SCREEN_WRAP (CreateWindow, xglCreateWindow);
    XGL_SCREEN_WRAP (DestroyWindow, xglDestroyWindow);
    XGL_SCREEN_WRAP (ChangeWindowAttributes, xglChangeWindowAttributes);
    XGL_SCREEN_WRAP (PaintWindowBackground, xglPaintWindowBackground);
    XGL_SCREEN_WRAP (PaintWindowBorder, xglPaintWindowBorder);

    XGL_SCREEN_WRAP (CreateGC, xglCreateGC);

#if 0
#define xglQueryBestSize	  (void *) NoopDDA
#define xglSaveScreen		  (void *) NoopDDA

#define xglConstrainCursor	  (void *) NoopDDA
#define xglCursorLimits		  (void *) NoopDDA
#define xglDisplayCursor	  (void *) NoopDDA
#define xglRealizeCursor	  (void *) NoopDDA
#define xglUnrealizeCursor	  (void *) NoopDDA
#define xglRecolorCursor	  (void *) NoopDDA
#define xglSetCursorPosition	  (void *) NoopDDA

    /* Might be nice to provide a textured hw cursor at some point */
    pScreen->ConstrainCursor   = xglConstrainCursor;
    pScreen->CursorLimits      = xglCursorLimits;
    pScreen->DisplayCursor     = xglDisplayCursor;
    pScreen->RealizeCursor     = xglRealizeCursor;
    pScreen->UnrealizeCursor   = xglUnrealizeCursor;
    pScreen->RecolorCursor     = xglRecolorCursor;
    pScreen->SetCursorPosition = xglSetCursorPosition;
#endif

    pScreen->ModifyPixmapHeader = xglModifyPixmapHeader;

    XGL_SCREEN_WRAP (BitmapToRegion, xglPixmapToRegion);

    pScreen->GetWindowPixmap = xglGetWindowPixmap;

    XGL_SCREEN_WRAP (SetWindowPixmap, xglSetWindowPixmap);

#ifdef RENDER
    pPictureScreen = GetPictureScreenIfSet (pScreen);
    if (pPictureScreen)
    {
	if (!AllocateGlyphPrivate (pScreen, xglGlyphPrivateIndex,
				   sizeof (xglGlyphRec)))
	    return FALSE;

	XGL_PICTURE_SCREEN_WRAP (Composite, xglComposite);
	XGL_PICTURE_SCREEN_WRAP (RealizeGlyph, xglRealizeGlyph);
	XGL_PICTURE_SCREEN_WRAP (UnrealizeGlyph, xglUnrealizeGlyph);
	XGL_PICTURE_SCREEN_WRAP (Glyphs, xglGlyphs);
	XGL_PICTURE_SCREEN_WRAP (Trapezoids, xglTrapezoids);
	XGL_PICTURE_SCREEN_WRAP (AddTraps, xglAddTraps);
	XGL_PICTURE_SCREEN_WRAP (AddTriangles, xglAddTriangles);
	XGL_PICTURE_SCREEN_WRAP (ChangePicture, xglChangePicture);
	XGL_PICTURE_SCREEN_WRAP (ChangePictureTransform,
				 xglChangePictureTransform);
	XGL_PICTURE_SCREEN_WRAP (ChangePictureFilter, xglChangePictureFilter);
    }
#endif

    XGL_SCREEN_WRAP (BackingStoreFuncs.SaveAreas, xglSaveAreas);
    XGL_SCREEN_WRAP (BackingStoreFuncs.RestoreAreas, xglRestoreAreas);

#if 0
#ifdef COMPOSITE
#warning "composite building"
    if (!compScreenInit (pScreen))
	return FALSE;
#endif
#endif

    /* Damage is required */
    DamageSetup (pScreen);

    XGL_SCREEN_WRAP (CloseScreen, glucoseCloseScreen);

    return TRUE;
}

static Bool
glucoseInitVisual (ScreenPtr	 pScreen,
	       xglVisualPtr	 pVisual,
	       xglPixelFormatPtr pPixel,
	       VisualID		 vid)
{
    glitz_format_t *format;

    XGL_SCREEN_PRIV (pScreen);

    format = xglFindBestSurfaceFormat (pScreen, pPixel);
    if (format)
    {
	glitz_drawable_format_t templ;
	unsigned long	        mask;

	templ.color        = format->color;
	templ.depth_size   = 0;
	templ.stencil_size = 0;
	templ.doublebuffer = 0;
	templ.samples      = 1;

	mask =
	    GLITZ_FORMAT_FOURCC_MASK       |
	    GLITZ_FORMAT_RED_SIZE_MASK     |
	    GLITZ_FORMAT_GREEN_SIZE_MASK   |
	    GLITZ_FORMAT_BLUE_SIZE_MASK    |
	    GLITZ_FORMAT_ALPHA_SIZE_MASK   |
	    GLITZ_FORMAT_DEPTH_SIZE_MASK   |
	    GLITZ_FORMAT_STENCIL_SIZE_MASK |
	    GLITZ_FORMAT_DOUBLEBUFFER_MASK |
	    GLITZ_FORMAT_SAMPLES_MASK;

	pVisual->next	 = 0;
	pVisual->vid	 = vid;
	pVisual->pPixel	 = pPixel;
	pVisual->pbuffer = FALSE;

	pVisual->format.surface  = format;
	pVisual->format.drawable =
	    glitz_find_drawable_format (pScreenPriv->drawable,
					mask, &templ, 0);

	return TRUE;
    }

    return FALSE;
}

static void
glucoseInitVisuals (ScreenPtr pScreen)
{
    xglVisualPtr v, new, *prev;
    xglPixelFormatPtr pPixel = NULL;
    int		 i,j;

    XGL_SCREEN_PRIV (pScreen);

    for (j = 0; j < pScreen->numVisuals; j++)
    {
        pPixel = NULL;
    	for (i = 0; i < xglNumPixelFormats(); i++)
        {
	    if (pScreen->visuals[j].nplanes == xglPixelFormats[i].depth &&
	    	BitsPerPixel(pScreen->visuals[j].nplanes) == xglPixelFormats[i].masks.bpp)
	    	pPixel = &xglPixelFormats[i];
	}
	if (pPixel) {
	    new = xalloc (sizeof (xglVisualRec));
	    if (new)
	    {
		if (glucoseInitVisual (pScreen, new, pPixel,
				   pScreen->visuals[j].vid))
		{
		    new->next = 0;

		    prev = &pScreenPriv->pVisual;
		    while ((v = *prev)) {
			prev = &v->next;
		    }

		    *prev = new;
		}
		else
		{
		    xfree (new);
		}
	    }
	}
    }
}

Bool
glucoseFinishScreenInit (ScreenPtr pScreen)
{
    xglVisualPtr v;

#ifdef RENDER
    glitz_vertex_format_t *format;
    static glitz_color_t  clearBlack = { 0x0, 0x0, 0x0, 0x0 };
    static glitz_color_t  solidWhite = { 0xffff, 0xffff, 0xffff, 0xffff };
    int			  i;
#endif

    XGL_SCREEN_PRIV (pScreen);

    glucoseInitVisuals (pScreen);

    for (v = pScreenPriv->pVisual; v; v = v->next)
    {
	if (v->vid == pScreen->rootVisual)
	    pScreenPriv->rootVisual = v;
    }

    if (!pScreenPriv->rootVisual || !pScreenPriv->rootVisual->format.surface)
	return FALSE;

    pScreenPriv->surface =
	glitz_surface_create (pScreenPriv->drawable,
			      pScreenPriv->rootVisual->format.surface,
			      pScreen->width, pScreen->height,
			      0, NULL);
    if (!pScreenPriv->surface)
	return FALSE;

    glitz_surface_attach (pScreenPriv->surface,
			  pScreenPriv->drawable,
			  GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

#ifdef RENDER
    for (i = 0; i < 33; i++)
	pScreenPriv->glyphCache[i].pScreen = NULL;

    for (v = pScreenPriv->pVisual; v; v = v->next)
    {
	if (v->pPixel->depth == 8)
	    break;
    }

    pScreenPriv->pSolidAlpha    = 0;
    pScreenPriv->trapInfo.pMask = 0;

    /* An accelerated alpha only Xgl visual is required for trapezoid
       acceleration */
    if (v && v->format.surface)
    {
	glitz_surface_t *mask;

	mask = glitz_surface_create (pScreenPriv->drawable,
				     v->format.surface,
				     2, 1, 0, NULL);
	if (mask)
	{
	    glitz_set_rectangle (mask, &clearBlack, 0, 0, 1, 1);
	    glitz_set_rectangle (mask, &solidWhite, 1, 0, 1, 1);

	    glitz_surface_set_fill (mask, GLITZ_FILL_NEAREST);
	    glitz_surface_set_filter (mask, GLITZ_FILTER_BILINEAR, NULL, 0);

	    pScreenPriv->trapInfo.pMask = xglCreateDevicePicture (mask);
	    if (!pScreenPriv->trapInfo.pMask)
		return FALSE;
	}
    }

    format = &pScreenPriv->trapInfo.format.vertex;
    format->primitive  = GLITZ_PRIMITIVE_QUADS;
    format->attributes = GLITZ_VERTEX_ATTRIBUTE_MASK_COORD_MASK;

    format->mask.type	     = GLITZ_DATA_TYPE_FLOAT;
    format->mask.size	     = GLITZ_COORDINATE_SIZE_X;
    format->bytes_per_vertex = sizeof (glitz_float_t);

    if (pScreenPriv->geometryDataType)
    {
	format->type		  = GLITZ_DATA_TYPE_FLOAT;
	format->bytes_per_vertex += 2 * sizeof (glitz_float_t);
	format->mask.offset	  = 2 * sizeof (glitz_float_t);
    }
    else
    {
	format->type		  = GLITZ_DATA_TYPE_SHORT;
	format->bytes_per_vertex += 2 * sizeof (glitz_short_t);
	format->mask.offset	  = 2 * sizeof (glitz_short_t);
    }
#endif

#if 0 /* Let the driver do this ! */
#ifdef XV
    if (!xglXvScreenInit (pScreen))
       return FALSE;
#endif
#endif

    return TRUE;
}

Bool
glucoseCloseScreen (int	  index,
		ScreenPtr pScreen)
{
    xglVisualPtr v;

    XGL_SCREEN_PRIV (pScreen);
    XGL_PIXMAP_PRIV (pScreenPriv->pScreenPixmap);
    XGL_SCREEN_UNWRAP (CloseScreen);

    {
    	GlucoseScreenPrivPtr pScreenPriv = GlucoseGetScreenPriv(pScreen);
    
    	pScreenPriv->rootContext->makeCurrent(pScreenPriv->rootContext);
    }

#ifdef RENDER
    int i;

    for (i = 0; i < 33; i++)
	xglFiniGlyphCache (&pScreenPriv->glyphCache[i]);

    if (pScreenPriv->pSolidAlpha)
	FreePicture ((pointer) pScreenPriv->pSolidAlpha, 0);

    if (pScreenPriv->trapInfo.pMask)
	FreePicture ((pointer) pScreenPriv->trapInfo.pMask, 0);
#endif

    xglFiniPixmap (pScreenPriv->pScreenPixmap);
    if (pPixmapPriv->pDamage)
	DamageDestroy (pPixmapPriv->pDamage);

    if (pScreenPriv->surface)
	glitz_surface_destroy (pScreenPriv->surface);
    pPixmapPriv->surface = NULL;
    pScreenPriv->surface = NULL;

    GEOMETRY_UNINIT (&pScreenPriv->scratchGeometry);

    if (pScreenPriv->drawable)
	glitz_drawable_destroy(pScreenPriv->drawable);
    pPixmapPriv->drawable = NULL;
    pScreenPriv->drawable = NULL;
    xglScreenInfo.drawable = NULL;

    while (pScreenPriv->pVisual)
    {
	v = pScreenPriv->pVisual;
	pScreenPriv->pVisual = v->next;
	xfree (v);
    }
    xfree(pScreenPriv);
    pScreenPriv = NULL;

    /* tear down glucose now */
    {
    	GlucoseScreenPrivPtr pScreenPriv = GlucoseGetScreenPriv(pScreen);

    	pScreenPriv->rootContext->destroy(pScreenPriv->rootContext);
    	pScreenPriv->rootDrawable->destroy(pScreenPriv->rootDrawable);

    	xfree(pScreenPriv);
	pScreenPriv = NULL;
    }

    return (*pScreen->CloseScreen) (index, pScreen);
}
