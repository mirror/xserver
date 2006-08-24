
XCBVISUALTYPE *xsGetVisual(VisualPtr pVisual);
XCBVISUALTYPE *xsGetVisualFromID(ScreenPtr pScreen, XCBVISUALID visual);
XCBVISUALTYPE *xsGetDefaultVisual(ScreenPtr pScreen);
XCBCOLORMAP xsDefaultVisualColormap(XCBVISUALTYPE *visual);
