#ifndef _XS_COLOR_INCL_ 
#define _XS_COLOR_INCL_

typedef struct {
  XCBCOLORMAP colormap;
} XscreenPrivColormap;

#define XS_CMAP_PRIV(pCmap) \
  ((XscreenPrivColormap *)((pCmap)->devPriv))

Bool xsCreateColormap(ColormapPtr pCmap);
void xsDestroyColormap(ColormapPtr pCmap);
void xsInstallColormap(ColormapPtr pCmap);
void xsUninstallColormap(ColormapPtr pCmap);
int  xsListInstalledColormaps(ScreenPtr pScreen, XCBCOLORMAP *pCmapIDs);
void xsStoreColors(ColormapPtr pCmap, int nColors, XCBCOLORITEM *pColors);
void xsResolveColor(CARD16 *r, CARD16 *g, CARD16 *b, VisualPtr pVisual);

//void xsSetInstalledColormapWindows(ScreenPtr pScreen);
//void xsSetScreenSaverColormapWindow(ScreenPtr pScreen);
//void xsDirectInstallColormaps(ScreenPtr pScreen);
//void xsDirectUninstallColormaps(ScreenPtr pScreen);
//Bool xsCreateDefaultColormap(ScreenPtr pScreen);

#endif /* XNESTCOLOR_H */
