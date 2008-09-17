/*
 * Copyright 2001-2004 Red Hat Inc., Durham, North Carolina.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL RED HAT AND/OR THEIR SUPPLIERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <kem@redhat.com>
 *
 */

/** \file
 * This file provides support for fonts. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxsync.h"
#include "dmxfont.h"
#include "dmxlog.h"

#include <X11/fonts/fontstruct.h>
#include "dixfont.h"
#include "dixstruct.h"

/** Load the font, \a pFont, on the back-end server associated with \a
 *  pScreen.  When a font is loaded, the font path on back-end server is
 *  first initialized to that specified on the command line with the
 *  -fontpath options, and then the font is loaded. */
Bool dmxBELoadFont(ScreenPtr pScreen, FontPtr pFont)
{
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxFontPrivPtr  pFontPriv = FontGetPrivate(pFont, dmxFontPrivateIndex);
    char           *name;
    Atom            name_atom, value_atom;
    int             i;

    /* Make sure we have a font private struct to work with */
    if (!pFontPriv)
	return FALSE;

    /* Don't load a font over top of itself */
    if (pFontPriv->font[pScreen->myNum]) {
	return TRUE; /* Already loaded font */
    }

    /* Find requested font on back-end server */
    name_atom = MakeAtom("FONT", 4, TRUE);
    value_atom = 0L;

    for (i = 0; i < pFont->info.nprops; i++) {
	if ((Atom)pFont->info.props[i].name == name_atom) {
	    value_atom = pFont->info.props[i].value;
	    break;
	}
    }
    if (!value_atom) return FALSE;

    name = (char *)NameForAtom(value_atom);
    if (!name) return FALSE;

    pFontPriv->font[pScreen->myNum] = None;

    XLIB_PROLOGUE (dmxScreen);
    pFontPriv->font[pScreen->myNum] =
	XLoadQueryFont(dmxScreen->beDisplay, name);
    XLIB_EPILOGUE (dmxScreen);

    dmxSync(dmxScreen, FALSE);

    if (!pFontPriv->font[pScreen->myNum]) return FALSE;

    return TRUE;
}

/** Realize the font, \a pFont, on the back-end server associated with
 *  \a pScreen. */
Bool dmxRealizeFont(ScreenPtr pScreen, FontPtr pFont)
{
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxFontPrivPtr  pFontPriv;

    if (!(pFontPriv = FontGetPrivate(pFont, dmxFontPrivateIndex))) {
	FontSetPrivate(pFont, dmxFontPrivateIndex, NULL);
	pFontPriv = xalloc(sizeof(dmxFontPrivRec));
	if (!pFontPriv) return FALSE;
        pFontPriv->font = NULL;
        MAXSCREENSALLOC(pFontPriv->font);
        if (!pFontPriv->font) {
            xfree(pFontPriv);
            return FALSE;
        }
	pFontPriv->refcnt = 0;
    }

    FontSetPrivate(pFont, dmxFontPrivateIndex, (pointer)pFontPriv);

    if (dmxScreen->beDisplay) {
	if (!dmxBELoadFont(pScreen, pFont))
	    return FALSE;

	pFontPriv->refcnt++;
    } else {
	pFontPriv->font[pScreen->myNum] = NULL;
    }

    return TRUE;
}

/** Free \a pFont on the back-end associated with \a pScreen. */
Bool dmxBEFreeFont(ScreenPtr pScreen, FontPtr pFont)
{
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxFontPrivPtr  pFontPriv = FontGetPrivate(pFont, dmxFontPrivateIndex);

    if (pFontPriv && pFontPriv->font[pScreen->myNum]) {
	XLIB_PROLOGUE (dmxScreen);
	XFreeFont(dmxScreen->beDisplay, pFontPriv->font[pScreen->myNum]);
	XLIB_EPILOGUE (dmxScreen);
	pFontPriv->font[pScreen->myNum] = NULL;
	return TRUE;
    }

    return FALSE;
}

/** Unrealize the font, \a pFont, on the back-end server associated with
 *  \a pScreen. */
Bool dmxUnrealizeFont(ScreenPtr pScreen, FontPtr pFont)
{
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxFontPrivPtr  pFontPriv;

    if ((pFontPriv = FontGetPrivate(pFont, dmxFontPrivateIndex))) {
	/* In case the font failed to load properly */
	if (!pFontPriv->refcnt) {
            MAXSCREENSFREE(pFontPriv->font);
	    xfree(pFontPriv);
	    FontSetPrivate(pFont, dmxFontPrivateIndex, NULL);
	} else if (pFontPriv->font[pScreen->myNum]) {
	    if (dmxScreen->beDisplay)
		dmxBEFreeFont(pScreen, pFont);

	    /* The code below is non-obvious, so here's an explanation...
	     *
	     * When creating the default GC, the server opens up the
	     * default font once for each screen, which in turn calls
	     * the RealizeFont function pointer once for each screen.
	     * During this process both dix's font refcnt and DMX's font
	     * refcnt are incremented once for each screen.
	     *
	     * Later, when shutting down the X server, dix shuts down
	     * each screen in reverse order.  During this shutdown
	     * procedure, each screen's default GC is freed and then
	     * that screen is closed by calling the CloseScreen function
	     * pointer.  screenInfo.numScreens is then decremented after
	     * closing each screen.  This procedure means that the dix's
	     * font refcnt for the font used by the default GC's is
	     * decremented once for each screen # greater than 0.
	     * However, since dix's refcnt for the default font is not
	     * yet 0 for each screen greater than 0, no call to the
	     * UnrealizeFont function pointer is made for those screens.
	     * Then, when screen 0 is being closed, dix's font refcnt
	     * for the default GC's font is finally 0 and the font is
	     * unrealized.  However, since screenInfo.numScreens has
	     * been decremented already down to 1, only one call to
	     * UnrealizeFont is made (for screen 0).  Thus, even though
	     * RealizeFont was called once for each screen,
	     * UnrealizeFont is only called for screen 0.
	     *
	     * This is a bug in dix.
	     *
	     * To avoid the memory leak of pFontPriv for each server
	     * generation, we can also free pFontPriv if the refcnt is
	     * not yet 0 but the # of screens is 1 -- i.e., the case
	     * described in the dix bug above.  This is only a temporary
	     * workaround until the bug in dix is solved.
	     *
	     * The other problem is that the font structure allocated by
	     * XLoadQueryFont() above is not freed for screens > 0.
	     * This problem cannot be worked around here since the back-
	     * end displays for screens > 0 have already been closed by
	     * the time this code is called from dix.
	     *
	     * When the bug in dix described above is fixed, then we can
	     * remove the "|| screenInfo.numScreens == 1" code below and
	     * the memory leaks will be eliminated.
	     */
	    if (--pFontPriv->refcnt == 0
#if 1
		/* Remove this code when the dix bug is fixed */
		|| screenInfo.numScreens == 1
#endif
		) {
                MAXSCREENSFREE(pFontPriv->font);
		xfree(pFontPriv);
		FontSetPrivate(pFont, dmxFontPrivateIndex, NULL);
	    }
	}
    }

    return TRUE;
}
