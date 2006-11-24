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

********************************************************/

/*
 * lgeint.h - Server-internal defines for LGE
 */

#ifndef _LGEINT_H
#define _LGEINT_H

#include "extnsionst.h"

/* Bump this number for every change to the LG3D code in the X Server */
#define LG3D_IMPLEMENTATION 	7

extern int lgeDisplayServerIsAlive;
extern ClientPtr lgePickerClient;
extern ClientPtr lgeEventDelivererClient;
extern Window lgeDisplayServerPRW;
extern WindowPtr pLgeDisplayServerPRWWin;
extern int lgeEventComesFromDS;
extern DeviceIntPtr lgekb;
extern DeviceIntPtr lgems;

extern Window lgeDisplayServerPRWsList[MAXSCREENS];
extern WindowPtr pLgeDisplayServerPRWWinsList[MAXSCREENS];
extern WindowPtr pLgeDisplayServerPRWWinRoots[MAXSCREENS];

#define IsWinLgePRWWin(pWin) \
    ((pWin) == pLgeDisplayServerPRWWinsList[(pWin)->drawable.pScreen->myNum])

#define GetLgePRWForRoot(pWin) \
    (pLgeDisplayServerPRWWinsList[(pWin)->drawable.pScreen->myNum])

extern int IsWinLgePRWOne(int win);
extern WindowPtr GetLgePRWWinFor(int win);

extern Bool lgeCompatibleExtension (ExtensionEntry *pExt);

#endif /* LGEINT_H */

