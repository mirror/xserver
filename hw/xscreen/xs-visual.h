
xcb_visualtype_t *xsGetVisual(VisualPtr pVisual);
xcb_visualtype_t *xsGetVisualFromID(ScreenPtr pScreen, xcb_visualid_t visual);
xcb_visualtype_t *xsGetDefaultVisual(ScreenPtr pScreen);
xcb_colormap_t xsDefaultVisualColormap(xcb_visualtype_t *visual);
