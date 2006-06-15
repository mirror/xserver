/* $Xorg: Font.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

   Copyright 1993 by Davor Matic

   Permission to use, copy, modify, distribute, and sell this software
   and its documentation for any purpose is hereby granted without fee,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation.  Davor Matic makes no representations about
   the suitability of this software for any purpose.  It is provided "as
   is" without express or implied warranty.

*/
/* $XFree86: xc/programs/Xserver/hw/xnest/Font.c,v 3.6 2003/07/16 01:38:51 dawes Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xproto.h>
#include "misc.h"
#include "regionstr.h"
#include <X11/fonts/font.h>
#include <X11/fonts/fontstruct.h>
#include "scrnintstr.h"

#include "Xnest.h"

#include "Display.h"
#include "XNFont.h"

int xnestFontPrivateIndex;

Bool xnestRealizeFont(ScreenPtr pScreen, FontPtr pFont)
{
    pointer priv;
    XCBFONT font;
    XCBATOM name_atom, value_atom;

    int nprops;
    FontPropPtr props;
    int i;
    char *name;

    FontSetPrivate(pFont, xnestFontPrivateIndex, NULL);

    if (requestingClient && XpClientIsPrintClient(requestingClient, NULL))
        return True;

    name_atom.xid = MakeAtom("FONT", 4, True);
    value_atom.xid = 0L;

    nprops = pFont->info.nprops;
    props = pFont->info.props;

    for (i = 0; i < nprops; i++)
        if (props[i].name == name_atom.xid) {
            value_atom.xid = props[i].value;
            break;
        }

    if (!value_atom.xid) return False;

    name = (char *)NameForAtom(value_atom.xid);

    if (!name) return False;

    priv = (pointer)xalloc(sizeof(xnestPrivFont));
    FontSetPrivate(pFont, xnestFontPrivateIndex, priv);

    font = XCBFONTNew(xnestConnection);
    xnestFontPriv(pFont)->font = font;
    XCBOpenFont(xnestConnection, font, strlen(name), name);

    if (!xnestFont(pFont).xid)
        return False;

    return True;
}


Bool xnestUnrealizeFont(ScreenPtr pScreen, FontPtr pFont)
{
    if (xnestFontPriv(pFont)) {
        if (xnestFont(pFont).xid) 
            XCBCloseFont(xnestConnection, xnestFont(pFont));
        xfree(xnestFontPriv(pFont));
        FontSetPrivate(pFont, xnestFontPrivateIndex, NULL);
    }
    return True;
}
