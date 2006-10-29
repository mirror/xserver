#ifdef HAVE_XS_CONFIG_H
#include <xs-config.h>
#endif
#include <X11/Xmd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/xcb_image.h>
#include "regionstr.h"
#include <X11/fonts/fontstruct.h>
#include "gcstruct.h"
#include "colormapst.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "region.h"
#include "servermd.h"


#include "xs-globals.h"
#include "xs-font.h"

Bool xsRealizeFont(ScreenPtr pScreen, FontPtr pFont)
{
    pointer priv;
    xcb_font_t font;
    xcb_atom_t name_atom, value_atom;

    int nprops;
    FontPropPtr props;
    int i;
    char *name;

    FontSetPrivate(pFont, xsFontPrivateIndex, NULL);

    name_atom = MakeAtom("FONT", 4, TRUE);
    value_atom = 0L;

    nprops = pFont->info.nprops;
    props = pFont->info.props;

    for (i = 0; i < nprops; i++) {
        if (props[i].name == name_atom) {
            value_atom = props[i].value;
            break;
        }
    }
    if (!value_atom)
        return FALSE;

    name = NameForAtom(value_atom);
    if (!name)
        return FALSE;

    priv = xalloc(sizeof(XscreenPrivFont));
    FontSetPrivate(pFont, xsFontPrivateIndex, priv);

    font = xcb_generate_id(xsConnection);
    XS_FONT_PRIV(pFont)->font = font;
    xcb_open_font(xsConnection, font, strlen(name), name);

    if (!XS_FONT_PRIV(pFont)->font)
        return FALSE;

    return TRUE;
}

Bool xsUnrealizeFont(ScreenPtr pScreen, FontPtr pFont)
{
    if (XS_FONT_PRIV(pFont)) {
        if (XS_FONT_PRIV(pFont)->font) 
            xcb_close_font(xsConnection, XS_FONT_PRIV(pFont)->font);
        xfree(XS_FONT_PRIV(pFont));
        FontSetPrivate(pFont, xsFontPrivateIndex, NULL);
    }
    return TRUE;
}
