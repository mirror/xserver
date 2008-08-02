/*
 * Copyright © 2008 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmxatom.h"

Atom
dmxAtom (DMXScreenInfo *dmxScreen,
	 Atom	       beAtom)
{
    Atom atom = None;

    if (beAtom < dmxScreen->beAtomTableSize)
	atom = dmxScreen->beAtomTable[beAtom];

    if (!atom)
    {
	char *name = NULL;

	XLIB_PROLOGUE (dmxScreen);
	name = XGetAtomName (dmxScreen->beDisplay, beAtom);
	XLIB_EPILOGUE (dmxScreen);

	if (!name)
	    return None;

	atom = MakeAtom (name, strlen (name), TRUE);
	if (!atom)
	    return None;

	if (beAtom >= dmxScreen->beAtomTableSize)
	{
	    Atom *table;
	    int  i;

	    table = xrealloc (dmxScreen->beAtomTable,
			      sizeof (Atom) * (beAtom + 1));
	    if (!table)
		return atom;

	    for (i = dmxScreen->beAtomTableSize; i < beAtom; i++)
		table[i] = None;

	    dmxScreen->beAtomTable     = table;
	    dmxScreen->beAtomTableSize = beAtom + 1;
	}

	dmxScreen->beAtomTable[beAtom] = atom;
    }

    return atom;
}

Atom
dmxBEAtom (DMXScreenInfo *dmxScreen,
	   Atom		 atom)
{
    Atom beAtom = None;

    if (atom < dmxScreen->atomTableSize)
	beAtom = dmxScreen->atomTable[atom];

    if (!beAtom)
    {
	char *name;

	name = NameForAtom (atom);
	if (!name)
	    return None;

	XLIB_PROLOGUE (dmxScreen);
	beAtom = XInternAtom (dmxScreen->beDisplay, name, FALSE);
	XLIB_EPILOGUE (dmxScreen);	

	if (atom >= dmxScreen->beAtomTableSize)
	{
	    Atom *table;
	    int  i;

	    table = xrealloc (dmxScreen->atomTable,
			      sizeof (Atom) * (atom + 1));
	    if (!table)
		return beAtom;

	    for (i = dmxScreen->atomTableSize; i < atom; i++)
		table[i] = None;

	    dmxScreen->atomTable     = table;
	    dmxScreen->atomTableSize = atom + 1;
	}

	dmxScreen->beAtomTable[atom] = beAtom;
    }

    return beAtom;
}
