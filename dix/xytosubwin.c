/************************************************************

Copyright (c) 2004, Sun Microsystems, Inc. 

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts,

			All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/

#ifdef LG3D

#include <X11/X.h>
#include <X11/Xproto.h>
#include "misc.h"
#include "resource.h"
#define NEED_EVENTS
#define NEED_REPLIES
#include "windowstr.h"
#include "inputstr.h"

extern WindowPtr XYToSubWindow (WindowPtr pWin, int x, int y, 
				int *xWinRel, int *yWinRel);
extern void fillSpriteTraceFromRootToWin (WindowPtr pWin);
extern Bool PointInBorderSize(WindowPtr pWin, int x, int y);

extern WindowPtr *spriteTrace;
extern int spriteTraceSize;
extern int spriteTraceGood;
extern WindowPtr lgeCurrentRootWin;

/* For Debug 
static void
printSpriteTrace (void) 
{
    int i;

    ErrorF("Sprite Trace: good = %d\n", spriteTraceGood);
    ErrorF("--------------------------------\n");
    for (i = 0; i < spriteTraceGood; i++) {
	ErrorF("%d: %d\n", i, (int)spriteTrace[i]->drawable.id);
    }
    ErrorF("--------------------------------\n");
}
*/

/*
** Fill sprite trace starting from pWin upward through the parent
** chain stopping at, and including, the root window. At this point
** the sprite trace is in the reverse order it should be, so before
** returning we reverse the order.
*/

void
fillSpriteTraceFromRootToWin (WindowPtr pWin)
{
    WindowPtr pParent;
    int       i;

    spriteTrace[0] = pWin;
    spriteTraceGood = 1;	

    pParent = pWin->parent;
    while (pParent != NULL) {
        if (spriteTraceGood >= spriteTraceSize) {
	    spriteTraceSize += 10;
	    Must_have_memory = TRUE; 
	    spriteTrace = (WindowPtr *)xrealloc(
	                  spriteTrace, spriteTraceSize*sizeof(WindowPtr));
    	    Must_have_memory = FALSE; 
	}
	spriteTrace[spriteTraceGood++] = pParent;
	pParent = pParent->parent;
    }

    /* For debug
    ErrorF("Before reversal\n");
    printSpriteTrace();
    */

    /* 
    ** Now reverse the order by swapping entries at either end,
    ** working our way toward the center
    */
    /*ErrorF("num swap loops = %d\n", (spriteTraceGood >> 1));*/
    for (i = 0; i < (spriteTraceGood >> 1); i++) {
	int       k = spriteTraceGood - i - 1;     
	/*ErrorF("Swap %d with %d\n", spriteTrace[i], spriteTrace[k]);*/
	WindowPtr pWinTmp = spriteTrace[i];
	spriteTrace[i] = spriteTrace[k];
	spriteTrace[k] = pWinTmp;
    }

    /* For debug
    ErrorF("After sprite trace fill:\n");
    printSpriteTrace();
    */
}

/*
** Starting from a specified window, the deepest descendent 
** in the tree which contains (x,y) (this point is in the
** coordinate space of pWin). This descendent is returned.
** (x,y) are in window relative coordinates.
**
** The sprite location relative to the sprite window is returned.
**
** This routine also appropriately updates the sprite trace 
** to contain a trace from the deepest descendent upward through 
** the parent chain to the root.
**
** Note: derived from events.c:XYToWindow
*/

WindowPtr 
XYToSubWindow (WindowPtr pWin, int x, int y, int *xWinRel, int *yWinRel)
{
    WindowPtr pSpriteWin;

    /* Convert to screen absolute coordinates */
    x += pWin->drawable.x;
    y += pWin->drawable.y;
    /*ErrorF("scr abs xy = %d, %d\n", x, y);*/

    /* Fill up the sprite trace to what we know so far */
    fillSpriteTraceFromRootToWin(pWin);

    /* Examine children of the window */
    pWin = pWin->firstChild;

    /* 
    ** From here on, the function is identical to XYToWindow
    */

    while (pWin)
    {
	/*
	if (pWin->mapped) {
	    ErrorF("Testing against win %d\n", pWin->drawable.id);
	    ErrorF("win rect (excl border) = %d %d %d %d\n", 
		   pWin->drawable.x, 
		   pWin->drawable.y,
		   pWin->drawable.x + pWin->drawable.width, 
		   pWin->drawable.y + pWin->drawable.height); 
	}
	*/

	if ((pWin->mapped) &&
		(x >= pWin->drawable.x - wBorderWidth (pWin)) &&
		(x < pWin->drawable.x + (int)pWin->drawable.width +
		    wBorderWidth(pWin)) &&
		(y >= pWin->drawable.y - wBorderWidth (pWin)) &&
		(y < pWin->drawable.y + (int)pWin->drawable.height +
		    wBorderWidth (pWin))
#ifdef SHAPE
		/* When a window is shaped, a further check
		 * is made to see if the point is inside
		 * borderSize
		 */
#ifndef NO_XINERAMA_PORT
		&& (!wBoundingShape(pWin) || PointInBorderSize(pWin, x, y))
    	        && (!wInputShape(pWin))
#else
		&& (!wBoundingShape(pWin) ||
		    POINT_IN_REGION(pWin->drawable.pScreen,
				    &pWin->borderSize, x, y, &box))
    	        && (!wInputShape(pWin))
#endif
#endif
		)
	{
	    /*ErrorF("Hit win %d\n", pWin->drawable.id);*/
	    if (spriteTraceGood >= spriteTraceSize)
	    {
		spriteTraceSize += 10;
		Must_have_memory = TRUE; /* XXX */
		spriteTrace = (WindowPtr *)xrealloc(
		    spriteTrace, spriteTraceSize*sizeof(WindowPtr));
		Must_have_memory = FALSE; /* XXX */
	    }
	    spriteTrace[spriteTraceGood++] = pWin;
	    pWin = pWin->firstChild;
	    /*ErrorF("Proceed to first child\n");*/
	}
	else {
	    pWin = pWin->nextSib;
	    /*ErrorF("Proceed to next sib\n");*/
	}
    }

    /* For debug 
    ErrorF("After subwin calculation:\n");
    printSpriteTrace();
    */

    /* always make the root from sprite (lgeCurrentRootWin defined in events.c) */
    spriteTrace[0] = lgeCurrentRootWin;

    pSpriteWin = spriteTrace[spriteTraceGood-1];
    *xWinRel = x - pSpriteWin->drawable.x;
    *yWinRel = y - pSpriteWin->drawable.y;
    /*ErrorF("xyWinRel = %d, %d\n", *xWinRel, *yWinRel);*/

    return pSpriteWin;
}

#endif /* LG3D */
