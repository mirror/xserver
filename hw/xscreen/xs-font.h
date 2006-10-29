#ifndef _XS_FONT_INCL_
#define _XS_FONT_INCL_

typedef struct {
    xcb_font_t font;
} XscreenPrivFont;


#define XS_FONT_PRIV(pFont) \
  ((XscreenPrivFont *)FontGetPrivate(pFont, xsFontPrivateIndex))

Bool xsRealizeFont(ScreenPtr pScreen, FontPtr pFont);
Bool xsUnrealizeFont(ScreenPtr pScreen, FontPtr pFont);


#endif
