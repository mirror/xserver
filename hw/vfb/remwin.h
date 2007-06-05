
/*****************************************************************

Copyright 2007 Sun Microsystems, Inc.

All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, provided that the above
copyright notice(s) and this permission notice appear in all copies of
the Software and that both the above copyright notice(s) and this
permission notice appear in supporting documentation.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Except as contained in this notice, the name of a copyright holder
shall not be used in advertising or otherwise to promote the sale, use
or other dealings in this Software without prior written authorization
of the copyright holder.

******************************************************************/

#ifndef REMWIN_H
#define REMWIN_H

#include "scrnintstr.h"
#include "regionstr.h"
#include "rwcomm.h"
#include "gcstruct.h"

/* Don't send any pixel buffers larger than 16KB */
#define PIXEL_BUF_MAX_NUM_BYTES (1024*16)
#define PIXEL_BUF_MAX_NUM_PIXELS (PIXEL_BUF_MAX_NUM_BYTES/4)

/* 
** The RLE algorithm used never creates more pixels than
** is in the original buffer, so the max number of pixels
** provides an upper bound on the maximum number of runs.
*/
#define RLE_BUF_MAX_NUM_BYTES PIXEL_BUF_MAX_NUM_BYTES
#define RLE_BUF_MAX_NUM_PIXELS (RLE_BUF_MAX_NUM_BYTES/4)

typedef struct remwin_scr_priv_rec {
    RwcommPtr                   pComm;
    OsTimerPtr                  outputTimer;
    CloseScreenProcPtr		CloseScreen;
    ScreenWakeupHandlerProcPtr 	WakeupHandler;
    CreateWindowProcPtr		CreateWindow;
    RealizeWindowProcPtr	RealizeWindow;
    UnrealizeWindowProcPtr	UnrealizeWindow;
    DestroyWindowProcPtr	DestroyWindow;
    PositionWindowProcPtr	PositionWindow;
    ResizeWindowProcPtr		ResizeWindow;
    ChangeWindowAttributesProcPtr ChangeWindowAttributes;
    CreateGCProcPtr             CreateGC;
    DisplayCursorProcPtr        DisplayCursor;
    SetCursorPositionProcPtr    SetCursorPosition;

    unsigned char *pPixelBuf;
    unsigned char *pRleBuf;

    /* List of managed o(top-level, drawable) windows */
    WindowPtr   pManagedWindows;

    /* 
    ** Which client currently has interactive control.
    ** -1 indicates no one has control.
    */
    int controller;

    /* Which pointer buttons the controller has pressed */
    int controllerButtonMask;
    
    /* The currently displayed cursor */
    CursorPtr pCursor;

    /* 
    ** The managed window the cursor is in. 
    ** NULL if cursor isn't shown 
    */
    WindowPtr pCursorWin;

    /* The cursor position (relative to pCursorWin) */
    int cursorX;
    int cursorY;

    /* The window manager client */
    ClientPtr pWmClient;

    /* 
    ** When this is -1 window moves and resizes are programmatic.
    ** (i.e. invoked by the application). When this is not -1
    ** window moves and resizes are being made by the user.
    */
    int configuringClient;

    /* True when any windows may have unsent dirty pixels */
    Bool windowsAreDirty;

} RemwinScreenPrivRec, *RemwinScreenPrivPtr;

#define REMWIN_GET_SCRPRIV(pScreen) \
    ((RemwinScreenPrivPtr)((pScreen)->devPrivates[remwinScreenIndex].ptr))

#define REMWIN_GET_WINPRIV(pWin) \
    ((RemwinWindowPrivPtr)(pWin)->devPrivates[remwinWinIndex].ptr);

/* DELETE: no longer used 
#define REMWIN_SET_WINPRIV(pWin, pWinPriv) \
    (pWin)->devPrivates[remwinWinIndex].ptr = (pointer)pWinPriv;
*/

typedef struct remwin_window_priv_rec *RemwinWindowPrivPtr;

typedef struct remwin_window_priv_rec {

    /* 
    ** The window has been dirtied. That is, there are updates to send
    ** to the client. (One or both of the regions has been updated).
    */
    Bool dirty;

    /* Whether the window has been mapped */
    Bool mapped;

    /* The area of the window that has been dirtied */
    RegionRec dirtyReg;

    /* The link to the next window (not winpriv!) */
    WindowPtr pWinNext;

} RemwinWindowPrivRec;

#define REMWIN_GET_GCPRIV(pGC) \
    ((RemwinGCPrivPtr)(pGC)->devPrivates[remwinGCIndex].ptr);

/* DELETE: no longer used 
#define REMWIN_SET_GCPRIV(pGC, pGCPriv) \
    (pWin)->devPrivates[remwinGCIndex].ptr = (pointer)pGCPriv;
*/

typedef struct remwin_gc_priv_rec *RemwinGCPrivPtr;

typedef struct remwin_gc_priv_rec {
    GCOps	    *wrapOps;	    /* wrapped ops */
    GCFuncs	    *wrapFuncs;	    /* wrapped funcs */
} RemwinGCPrivRec;

extern RemwinScreenPrivRec remwinScreenPriv;

extern void rwoutTimerCreate (ScreenPtr pScreen);
extern void rwoutTimerDestroy (ScreenPtr pScreen);

extern void rwinHandler (ScreenPtr pScreen);

/* Maximum pixel output rate is 30 fps */
#define MAX_OUTPUT_RATE 30.0
#define TIMER_MS ((int)((1/MAX_OUTPUT_RATE) * 1000.0))

extern Bool rwoutInitScreen (ScreenPtr pScreen);

extern void rwTakeControlInit (ScreenPtr pScreen);
extern void rwTakeControl (ScreenPtr pScreen, int clientId, Bool steal);

extern int remwinScreenIndex;

extern Bool rwoutSetWindowTitlesWrite (ScreenPtr pScreen, int strLen, char *buf);

extern void rwoutBeep (int percent, DeviceIntPtr pDevice,
		       pointer ctrl, int unused);

extern void rwoutSyncClientOnConnect (ScreenPtr pScreen, RwcommClientPtr pCommClient);

extern Bool rlePixelsWrite (ScreenPtr pScreen, WindowPtr pWin, int x, int y, int w, int h);

extern Bool vfbRemoteWindow;

#endif /* REMWIN_H */
