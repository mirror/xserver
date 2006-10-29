#ifndef _XS_GLOBALS_INCL_
#define _XS_GLOBALS_INCL_

#define MAXDEPTH 32

typedef enum {
    XS_OWNED,
    XS_UNOWNED,
} XSOwnership;

#define XS_DRAWABLE_ID(/*DrawablePtr*/ d) \
    (((d)->type == DRAWABLE_WINDOW)? \
     ((xcb_drawable_t) (XS_WINDOW_PRIV((WindowPtr)(d))->window)) : \
     ((xcb_drawable_t) (XS_PIXMAP_PRIV((PixmapPtr)(d))->pixmap)))

extern xcb_connection_t *xsConnection;
extern xcb_drawable_t    xsDefaultDrawables[MAXDEPTH];
extern xcb_drawable_t    xsBackingRoot;
extern int            xsFontPrivateIndex;
extern int            xsGCPrivateIndex;
extern int            xsWindowPrivateIndex;
void xsAllocPrivateIndecies(ScreenPtr pScreen);
#endif
