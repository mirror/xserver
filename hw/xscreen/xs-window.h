#ifndef _XS_WINDOW_INCL_
#define _XS_WINDOW_INCL_

extern int xsWindowPrivateIndex;

typedef struct {
  xcb_window_t window;
  XSOwnership owned;
} XscreenPrivWindow;

/**
 * returns the window privates
 **/
#define XS_WINDOW_PRIV(pWin) \
    ((XscreenPrivWindow *)((pWin)->devPrivates[xsWindowPrivateIndex].ptr))
/**
 * returns whether the window in question is the root window.
 * NB: This ONLY works for screen 0, which is all I currently care about.
 **/
#define XS_IS_ROOT(pWin) \
    ((pWin) == (WindowTable[0]))


Bool xsCreateWindow(WindowPtr pWin);
Bool xsDestroyWindow(WindowPtr pWin);
Bool xsChangeWindowAttributes(WindowPtr pWin, unsigned long mask);
Bool xsRealizeWindow(WindowPtr pWin);
Bool xsUnrealizeWindow(WindowPtr pWin);
Bool xsPaintWindowBorder(WindowPtr pWin, RegionPtr pRegion, int what);
void xsPaintWindowBackground(WindowPtr pWin, RegionPtr pRegion, int what);
Bool xsPositionWindow(WindowPtr pWin, int x, int y);
void xsConfigureWindow(WindowPtr pWin, uint32_t mask);

void xsWindowExposures(WindowPtr pWin, RegionPtr pRgn, RegionPtr pOther);
void xsCopyWindow(WindowPtr pWin, xPoint old_orig, RegionPtr old_rgn);
void xsClipNotify(WindowPtr pWin, int dx, int dy);

#endif
