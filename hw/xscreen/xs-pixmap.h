#ifndef _XS_PIXMAP_INCL_
#define _XS_PIXMAP_INCL_

#ifdef PIXPRIV
extern int xsPixmapPrivateIndex;
#endif

typedef struct {
  xcb_pixmap_t pixmap;
  XSOwnership owned;
} XscreenPrivPixmap;

#ifdef PIXPRIV
#define XS_PIXMAP_PRIV(pPixmap) \
  ((XscreenPrivPixmap *)((pPixmap)->devPrivates[xsPixmapPrivateIndex].ptr))
#else
#define XS_PIXMAP_PRIV(pPixmap) \
  ((XscreenPrivPixmap *)((pPixmap)->devPrivate.ptr))
#endif

void      xsInitFormats(void);
int       xsPixmapCalcSize(int depth, int w, int h);
PixmapPtr xsCreatePixmap(ScreenPtr pScreen, int width, int height, int depth);
Bool      xsDestroyPixmap(PixmapPtr pPixmap);
RegionPtr xsPixmapToRegion(PixmapPtr pPixmap);

#endif
