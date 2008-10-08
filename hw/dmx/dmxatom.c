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
#include "dmxsync.h"
#include "dmxscrinit.h"

static void
dmxAddAtom (DMXScreenInfo *dmxScreen,
	    Atom          atom,
	    Atom          beAtom)
{
    int i;

    if (atom >= dmxScreen->beAtomTableSize)
    {
	DMXAtom *table;

	table = xrealloc (dmxScreen->beAtomTable,
			  sizeof (DMXAtom) * (atom + 1));
	if (table)
	{
	    for (i = dmxScreen->beAtomTableSize; i < atom; i++)
	    {
		table[i].atom            = None;
		table[i].cookie.sequence = 0;
	    }

	    table[atom].atom            = beAtom;
	    table[atom].cookie.sequence = 0;

	    dmxScreen->beAtomTable     = table;
	    dmxScreen->beAtomTableSize = atom + 1;
	}
    }
    else
    {
	dmxScreen->beAtomTable[atom].atom = beAtom;
    }

    if (beAtom)
    {
	if (beAtom >= dmxScreen->atomTableSize)
	{
	    Atom *table;

	    table = xrealloc (dmxScreen->atomTable,
			      sizeof (Atom) * (beAtom + 1));
	    if (table)
	    {
		for (i = dmxScreen->atomTableSize; i < beAtom; i++)
		    table[i] = None;

		table[beAtom] = atom;

		dmxScreen->atomTable     = table;
		dmxScreen->atomTableSize = beAtom + 1;
	    }
	}
	else
	{
	    dmxScreen->atomTable[beAtom] = atom;
	}
    }
}

static void
dmxInternAtomReply (ScreenPtr           pScreen,
		    unsigned int        sequence,
		    xcb_generic_reply_t *reply,
		    xcb_generic_error_t *error,
		    void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Atom          atom = (Atom) data;

    dmxScreen->beAtomTable[atom].cookie.sequence = 0;

    if (reply)
    {
	xcb_intern_atom_reply_t *xatom = (xcb_intern_atom_reply_t *) reply;

	dmxAddAtom (dmxScreen, atom, xatom->atom);
    }
}

static unsigned int
dmxBERequestAtom (DMXScreenInfo *dmxScreen,
		  Atom	        atom)
{
    if (atom < dmxScreen->beAtomTableSize)
	if (dmxScreen->beAtomTable[atom].atom)
	    return 0;

    if (atom <= XA_LAST_PREDEFINED)
    {
	dmxAddAtom (dmxScreen, atom, atom);
    }
    else
    {
	char *name;

	dmxAddAtom (dmxScreen, atom, None);

	name = NameForAtom (atom);
	if (name && atom < dmxScreen->beAtomTableSize)
	{
	    dmxScreen->beAtomTable[atom].cookie =
		xcb_intern_atom (dmxScreen->connection,
				 FALSE,
				 strlen (name),
				 name);
	    
	    return dmxScreen->beAtomTable[atom].cookie.sequence;
	}
    }

    return 0;
}

Atom
dmxAtom (DMXScreenInfo *dmxScreen,
	 Atom	       beAtom)
{
    if (beAtom < dmxScreen->atomTableSize)
	if (dmxScreen->atomTable[beAtom])
	    return dmxScreen->atomTable[beAtom];

    if (beAtom <= XA_LAST_PREDEFINED)
    {
	dmxAddAtom (dmxScreen, beAtom, beAtom);
    }
    else
    {
	xcb_get_atom_name_reply_t *reply;
	Atom                      atom;

	reply = xcb_get_atom_name_reply
	    (dmxScreen->connection,
	     xcb_get_atom_name (dmxScreen->connection,
				beAtom),
	     NULL);
	if (!reply)
	    return None;

	atom = MakeAtom ((char *) (reply + 1), reply->name_len, TRUE);

	free (reply);

	if (!atom)
	    return None;

	dmxAddAtom (dmxScreen, atom, beAtom);
    }

    if (beAtom < dmxScreen->atomTableSize)
	return dmxScreen->atomTable[beAtom];

    return None;
}

Atom
dmxBEAtom (DMXScreenInfo *dmxScreen,
	   Atom		 atom)
{
    xcb_intern_atom_cookie_t cookie = { 0 };

    if (atom < dmxScreen->beAtomTableSize)
    {
	if (dmxScreen->beAtomTable[atom].atom)
	    return dmxScreen->beAtomTable[atom].atom;

	cookie = dmxScreen->beAtomTable[atom].cookie;
    }

    if (!cookie.sequence)
	cookie.sequence = dmxBERequestAtom (dmxScreen, atom);
    
    if (cookie.sequence)
    {
	xcb_intern_atom_reply_t *reply;

	reply = xcb_intern_atom_reply (dmxScreen->connection, cookie, NULL);

	dmxInternAtomReply (screenInfo.screens[dmxScreen->index],
			    cookie.sequence,
			    (xcb_generic_reply_t *) reply,
			    NULL,
			    (void *) atom);
    }

    if (atom < dmxScreen->beAtomTableSize)
	return dmxScreen->beAtomTable[atom].atom;

    return None;
}

void
dmxBEPrefetchAtom (DMXScreenInfo *dmxScreen,
		   Atom	         atom)
{
    unsigned int sequence;

    sequence = dmxBERequestAtom (dmxScreen, atom);
    if (sequence)
	dmxAddRequest (&dmxScreen->request,
		       dmxInternAtomReply,
		       sequence,
		       (void *) atom);
}
