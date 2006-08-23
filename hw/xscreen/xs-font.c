#ifdef HAVE_XS_CONFIG_H
#include <xs-config.h>
#endif
#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_image.h>
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
    XCBFONT font;
    XCBATOM name_atom, value_atom;

    int nprops;
    FontPropPtr props;
    int i;
    char *name;

    FontSetPrivate(pFont, xsFontPrivateIndex, NULL);

    name_atom.xid = MakeAtom("FONT", 4, TRUE);
    value_atom.xid = 0L;

    nprops = pFont->info.nprops;
    props = pFont->info.props;

    for (i = 0; i < nprops; i++) {
        if (props[i].name == name_atom.xid) {
            value_atom.xid = props[i].value;
            break;
        }
    }
    if (!value_atom.xid)
        return FALSE;

    name = NameForAtom(value_atom.xid);
    if (!name)
        return FALSE;

    priv = xalloc(sizeof(XscreenPrivFont));
    FontSetPrivate(pFont, xsFontPrivateIndex, priv);

    font = XCBFONTNew(xsConnection);
    XS_FONT_PRIV(pFont)->font = font;
    XCBOpenFont(xsConnection, font, strlen(name), name);

    if (!XS_FONT_PRIV(pFont)->font.xid)
        return FALSE;

    return TRUE;
}

Bool xsUnrealizeFont(ScreenPtr pScreen, FontPtr pFont)
{
    if (XS_FONT_PRIV(pFont)) {
        if (XS_FONT_PRIV(pFont)->font.xid) 
            XCBCloseFont(xsConnection, XS_FONT_PRIV(pFont)->font);
        xfree(XS_FONT_PRIV(pFont));
        FontSetPrivate(pFont, xsFontPrivateIndex, NULL);
    }
    return TRUE;
}
