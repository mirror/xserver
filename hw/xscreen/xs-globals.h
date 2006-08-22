#ifndef _XS_GLOBALS_INCL_
#define _XS_GLOBALS_INCL_

#define MAXDEPTH 32

typedef enum {
    XS_OWNED,
    XS_UNOWNED,
} XSOwnership;

#define XS_DRAWABLE_ID(/*DrawablePtr*/ d) \
    (((d)->type == DRAWABLE_WINDOW)? \
     ((XCBDRAWABLE) (XS_WINDOW_PRIV((WindowPtr)(d))->window)) : \
     ((XCBDRAWABLE) (XS_PIXMAP_PRIV((PixmapPtr)(d))->pixmap)))

extern XCBConnection *xsConnection;
extern XCBDRAWABLE    xsDefaultDrawables[MAXDEPTH];
extern XCBDRAWABLE    xsDefaultWindow;
extern int            xsFontPrivateIndex;
extern int            xsGCPrivateIndex;
extern int            xsWindowPrivateIndex;

#endif
