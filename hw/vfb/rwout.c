#undef VERBOSE

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

#include <unistd.h>
#include "regionstr.h"
#include "damage.h"
#include "windowstr.h"
#include "remwin.h"
#include "rwcomm.h"
#include "misc.h"
#include "protocol.h"
#include "cursorstr.h"
#include "servermd.h"

/* TODO: replace with DS */
#include "rwcommsock.h"

#define USE_RLE_ENV_VAR "XREMWIN_USE_RLE"
#define VERIFY_RLE_ENV_VAR "XREMWIN_VERIFY_RLE"

static Bool useRle24 = FALSE;
Bool rwRleVerifyPixels = FALSE;

static unsigned long remwinGeneration = 0;
int remwinScreenIndex;
int remwinWinIndex;
int remwinGCIndex;

/* TODO: note: dix extension */
extern WindowPtr dixGetSpriteWindow (void);

static Bool rwoutCreateWindowWrite (ScreenPtr pScreen, WindowPtr pWin);
static Bool rwoutDestroyWindowWrite (ScreenPtr pScreen, WindowPtr pWin);
static Bool rwoutShowWindowWrite (ScreenPtr pScreen, WindowPtr pWin, int show);
static Bool rwoutConfigureWindowWrite (ScreenPtr pScreen, WindowPtr pWin, int x, int y, 
				       unsigned int w, unsigned int h, WindowPtr pSib);
static Bool rwoutPositionWindowWrite (ScreenPtr pScreen, WindowPtr pWin, int x, int y);
static Bool rwoutWindowSetDecoratedWrite (ScreenPtr pScreen, WindowPtr pWin);
static Bool rwoutWindowSetBorderWidthWrite (ScreenPtr pScreen, WindowPtr pWin);
static Bool rwoutWindowPixelsWrite (ScreenPtr pScreen, WindowPtr pWin, 
				    RemwinWindowPrivPtr pWinPriv);
static Bool rwoutPixelsWrite (ScreenPtr pScreen);
static Bool rwoutUncodedPixelsWrite (ScreenPtr pScreen, WindowPtr pWin, 
				     int x, int y, int w, int h);
static Bool rwoutDisplayCursorWrite (ScreenPtr pScreen, CursorPtr pCursor);
static Bool rwoutMoveCursorWrite (ScreenPtr pScreen, WindowPtr, int x, int y);
static Bool rwoutShowCursorWrite (ScreenPtr pScreen, Bool show);

static Bool rwoutCreateWindow(WindowPtr); 
static Bool rwoutRealizeWindow(WindowPtr); 
static Bool rwoutUnrealizeWindow(WindowPtr); 
static Bool rwoutDestroyWindow(WindowPtr);
static void rwoutResizeWindow(WindowPtr, int x, int y, 
			      unsigned int w, unsigned int h, WindowPtr pSib);
static Bool rwoutPositionWindow(WindowPtr, int x, int y);
static int rwoutChangeWindowAttributes(WindowPtr, Mask vmask);
static Bool rwoutDisplayCursor (ScreenPtr pScreen, CursorPtr pCursor);
static Bool rwoutSetCursorPosition (ScreenPtr pScreen, int x, int y,
				    Bool generateEvent);

static CARD32 rwoutTimerCallback (OsTimerPtr timer, CARD32 now, pointer arg);

/* TODO: do I really need to wrap them all? */

static Bool rwoutCreateGC (GCPtr pGC);
static void rwoutValidateGC(GCPtr pGC, unsigned long stateChanges, DrawablePtr pDrawable);
static void rwoutChangeGC (GCPtr pGC, unsigned long mask);
static void rwoutCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst);
static void rwoutDestroyGC(GCPtr pGC);
static void rwoutChangeClip(GCPtr pGC, int type, pointer pvalue, int nrects);
static void rwoutCopyClip(GCPtr pgcDst, GCPtr pgcSrc);
static void rwoutDestroyClip(GCPtr pGC);

static void rwoutFillSpans(DrawablePtr pDst, GCPtr pGC, int nInit,
			DDXPointPtr pptInit, int *pwidthInit, int fSorted);
static void rwoutSetSpans(DrawablePtr pDst, GCPtr pGC, char *psrc,
		       DDXPointPtr ppt, int *pwidth, int nspans, int fSorted);
static void rwoutPutImage(DrawablePtr pDst, GCPtr pGC, int depth,
		       int x, int y, int w, int h, int leftPad, int format,
		       char *pBits);
static RegionPtr rwoutCopyArea(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
			    int srcx, int srcy, int w, int h,
			    int dstx, int dsty);
static RegionPtr rwoutCopyPlane(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
			     int srcx, int srcy, int w, int h,
			     int dstx, int dsty, unsigned long plane);
static void rwoutPolyPoint(DrawablePtr pDst, GCPtr pGC, int mode, int npt,
			xPoint *pptInit);
static void rwoutPolylines(DrawablePtr pDst, GCPtr pGC, int mode, int npt,
			DDXPointPtr pptInit);
static void rwoutPolySegment(DrawablePtr pDst, GCPtr pGC, int nseg,
			  xSegment *pSegs);
static void rwoutPolyRectangle(DrawablePtr pDst, GCPtr pGC,
			    int nrects, xRectangle *pRects);
static void rwoutPolyArc(DrawablePtr pDst, GCPtr pGC, int narcs, xArc *parcs);
static void rwoutFillPolygon(DrawablePtr pDst, GCPtr pGC, int shape, int mode,
			  int count, DDXPointPtr pPts);
static void rwoutPolyFillRect(DrawablePtr pDst, GCPtr pGC,
			   int nrectFill, xRectangle *prectInit);
static void rwoutPolyFillArc(DrawablePtr pDst, GCPtr pGC,
			  int narcs, xArc *parcs);
static int rwoutPolyText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
		       int count, char *chars);
static int rwoutPolyText16(DrawablePtr pDst, GCPtr pGC, int x, int y,
			int count, unsigned short *chars);
static void rwoutImageText8(DrawablePtr pDst, GCPtr pGC, int x, int y,
			 int count, char *chars);
static void rwoutImageText16(DrawablePtr pDst, GCPtr pGC, int x, int y,
			  int count, unsigned short *chars);
static void rwoutImageGlyphBlt(DrawablePtr pDst, GCPtr pGC, int x, int y,
			    unsigned int nglyph, CharInfoPtr *ppci,
			    pointer pglyphBase);
static void rwoutPolyGlyphBlt(DrawablePtr pDst, GCPtr pGC, int x, int y,
			   unsigned int nglyph, CharInfoPtr *ppci,
			   pointer pglyphBase);
static void rwoutPushPixels(GCPtr pGC, PixmapPtr pBitMap, DrawablePtr pDst,
			 int w, int h, int x, int y);

#define PROLOG(pScrPriv, field)		\
    (pScrPriv) = REMWIN_GET_SCRPRIV(pScreen); \
    pScreen->field = (pScrPriv)->field;

#define EPILOG(field, wrapper) \
    pScreen->field = wrapper;

#define FUNC_PROLOG(pGC, pGCPriv) \
    (pGC)->funcs = (pGCPriv)->wrapFuncs; \
    (pGC)->ops = (pGCPriv)->wrapOps; 

#define FUNC_EPILOG(pGC, pGcPriv) \
    (pGCPriv)->wrapFuncs = (pGC)->funcs; \
    (pGCPriv)->wrapOps = (pGC)->ops; \
    (pGC)->funcs = &rwoutGCFuncs; \
    (pGC)->ops = &rwoutGCOps;

#define OPS_PROLOG(pGC) \
    pGC->funcs = pGCPriv->wrapFuncs; \
    pGC->ops = pGCPriv->wrapOps;

#define OPS_EPILOG(pGC) \
    pGCPriv->wrapFuncs = (pGC)->funcs; \
    pGCPriv->wrapOps = (pGC)->ops; \
    (pGC)->funcs = &rwoutGCFuncs; \
    (pGC)->ops = &rwoutGCOps; 

#define TOP_LEVEL_WIN(pWin) \
    ((pWin)->parent != NULL && (pWin)->parent->parent == NULL)

/* TODO: not yet supporting input only windows */
#define MANAGED_WIN(pWin) \
    ((pWin)->drawable.type == DRAWABLE_WINDOW && \
     TOP_LEVEL_WIN(pWin))

/* An optimzation: perform all CopyAreas on the client side */
static Bool clientSideCopies = FALSE;

static Bool ignoreCopyAreaDamage = FALSE;

static Bool rwoutCloseScreen (int i, ScreenPtr pScreen);

static GCFuncs rwoutGCFuncs = {
    rwoutValidateGC,
    rwoutChangeGC,
    rwoutCopyGC,
    rwoutDestroyGC,
    rwoutChangeClip,
    rwoutDestroyClip,
    rwoutCopyClip,
};

static GCOps rwoutGCOps = {
    rwoutFillSpans,
    rwoutSetSpans,
    rwoutPutImage,
    rwoutCopyArea,
    rwoutCopyPlane,
    rwoutPolyPoint,
    rwoutPolylines,
    rwoutPolySegment,
    rwoutPolyRectangle,
    rwoutPolyArc,
    rwoutFillPolygon,
    rwoutPolyFillRect,
    rwoutPolyFillArc,
    rwoutPolyText8,
    rwoutPolyText16,
    rwoutImageText8,
    rwoutImageText16,
    rwoutImageGlyphBlt,
    rwoutPolyGlyphBlt,
    rwoutPushPixels
};

static void
rwoutWakeupHandler (int screenNum, pointer wakeupData,
		    unsigned long result, pointer pReadmask)
{
    ScreenPtr pScreen = screenInfo.screens[screenNum];
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;

    if ((int)result >= 0) {
	RWCOMM_CLIENT_MESSAGE_POLL(pComm, pReadMask);
    }

    pScreen->WakeupHandler = pScrPriv->WakeupHandler;
    (*pScreen->WakeupHandler)(screenNum, wakeupData, result, 
			      pReadmask);
    pScreen->WakeupHandler = rwoutWakeupHandler;
}

Bool
rwoutInitScreen (ScreenPtr pScreen) {

    RemwinScreenPrivPtr pScrPriv;
    RwcommPtr pComm;

    /* 
    ** Workaround for a netbeans black window bug caused by a bug in Java 6 AWT.
    ** This is fixed in Java 6 update 1 but not all Wonderland users are using
    ** that version. 
    **
    ** TODO: eventually can remove this.
    */
    SetVendorString("The X.Org Foundation and Sun Microsystems - an eXcursion into Wonderland");

    if (remwinGeneration != serverGeneration)
    {
	remwinScreenIndex = AllocateScreenPrivateIndex();
	if (remwinScreenIndex < 0) {
	    return FALSE;
	}
	remwinWinIndex = AllocateWindowPrivateIndex();
	if (remwinWinIndex < 0) {
	    return FALSE;
	}
	remwinGCIndex = AllocateGCPrivateIndex();
	if (remwinGCIndex < 0) {
	    return FALSE;
	}
	remwinGeneration = serverGeneration;
    }

    if (!AllocateWindowPrivate(pScreen, remwinWinIndex, sizeof(RemwinWindowPrivRec))) {
	return FALSE;
    }

    if (!AllocateGCPrivate(pScreen, remwinGCIndex, sizeof(RemwinGCPrivRec))) {
	return FALSE;
    }

    pScrPriv = (RemwinScreenPrivPtr)xalloc(sizeof(RemwinScreenPrivRec));
    if (pScrPriv == NULL) {
	return FALSE;
    }
    pScreen->devPrivates[remwinScreenIndex].ptr = (pointer)pScrPriv;

    /* TODO: replace with ds */
    pComm = rwcommsockCreate(pScreen);
    if (pComm == NULL) {
	return FALSE;
    }
    pScrPriv->pComm = pComm;
    
    pScrPriv->CloseScreen = pScreen->CloseScreen;
    pScrPriv->WakeupHandler = pScreen->WakeupHandler;
    pScrPriv->CreateWindow = pScreen->CreateWindow;
    pScrPriv->RealizeWindow = pScreen->RealizeWindow;
    pScrPriv->UnrealizeWindow = pScreen->UnrealizeWindow;
    pScrPriv->DestroyWindow = pScreen->DestroyWindow;
    pScrPriv->PositionWindow = pScreen->PositionWindow;
    pScrPriv->ResizeWindow = pScreen->ResizeWindow;
    pScrPriv->ChangeWindowAttributes = pScreen->ChangeWindowAttributes;
    pScrPriv->CreateGC = pScreen->CreateGC;
    pScrPriv->DisplayCursor = pScreen->DisplayCursor;
    pScrPriv->SetCursorPosition = pScreen->SetCursorPosition;

    pScreen->CloseScreen = rwoutCloseScreen;
    pScreen->WakeupHandler = rwoutWakeupHandler;
    pScreen->CreateWindow = rwoutCreateWindow;
    pScreen->DestroyWindow = rwoutDestroyWindow;
    pScreen->RealizeWindow = rwoutRealizeWindow;
    pScreen->UnrealizeWindow = rwoutUnrealizeWindow;
    pScreen->ResizeWindow = rwoutResizeWindow;
    pScreen->PositionWindow = rwoutPositionWindow;
    pScreen->ChangeWindowAttributes = rwoutChangeWindowAttributes;
    pScreen->CreateGC = rwoutCreateGC;
    pScreen->DisplayCursor = rwoutDisplayCursor;
    pScreen->SetCursorPosition = rwoutSetCursorPosition;

    pScrPriv->pPixelBuf = NULL;
    pScrPriv->pRleBuf = NULL;
    pScrPriv->pManagedWindows = NULL;

    pScrPriv->pWmClient = NULL;
    pScrPriv->windowsAreDirty = FALSE;

    { 
      char *ev = getenv(USE_RLE_ENV_VAR);
      if (ev != NULL && !strcmp(ev, "true")) {
	  useRle24 = TRUE;
	  ErrorF("XRemWin: enabling run-length encoding\n");
      }

      ev = getenv(VERIFY_RLE_ENV_VAR);
      if (ev != NULL && !strcmp(ev, "true")) {
	  rwRleVerifyPixels = TRUE;
	  ErrorF("XRemWin: enabling RLE verification\n");
      }
    }

    return TRUE;
}

static Bool
rwoutCloseScreen (int i, ScreenPtr pScreen)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    WindowPtr pWin;
    RemwinWindowPrivPtr pWinPriv;
    int status;

    PROLOG(pScrPriv, CloseScreen);
    status = (*pScreen->CloseScreen)(i, pScreen);
    EPILOG(CloseScreen, rwoutCloseScreen);
    if (!status) {
	return FALSE;
    }
    
    if (pScrPriv->pPixelBuf != NULL) {
	xfree(pScrPriv->pPixelBuf);
    }
    if (pScrPriv->pRleBuf != NULL) {
	xfree(pScrPriv->pRleBuf);
    }

    pWin = pScrPriv->pManagedWindows;
    while (pWin != NULL) {
	pWinPriv = REMWIN_GET_WINPRIV(pWin);
	pWin = pWinPriv->pWinNext;
	REGION_UNINIT(pScreen, &pWinPriv->dirtyReg);
	xfree(pWinPriv);
    }

    rwoutTimerDestroy(pScreen);

    RWCOMM_DESTROY(pScrPriv->pComm);
    pScrPriv->pComm = NULL;

    return status;
}

/* 
** Dirty the entire window area (including the border).
*/

extern RESTYPE DamageExtType;

typedef struct _rwoutDamage {
    DamagePtr		pDamage;
    DrawablePtr		pDrawable;
    DamageReportLevel	level;
    XID			id;
} RwoutDamageRec, *RwoutDamagePtr;

static void
rwoutDamageReport (DamagePtr pDa, RegionPtr pRegion, void *closure)
{
    RwoutDamagePtr pDamage = closure;
    WindowPtr pWin = (WindowPtr) pDamage->pDrawable;
    RemwinWindowPrivPtr pWinPriv;
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    /* 
    ** If copying on the client side ignore any damage generated
    ** while inside CopyArea functions.
    */
    if (ignoreCopyAreaDamage) {
	return;
    }

    /*
    ScreenPtr pScreen = pWin->drawable.pScreen;
    PrintRegion(pScreen, pRegion);
    ErrorF("\n");
    */

    pWinPriv = REMWIN_GET_WINPRIV(pWin);		       
    REGION_UNION(pScreen, &pWinPriv->dirtyReg, &pWinPriv->dirtyReg, pRegion); 

    /*
    ErrorF("Modified region = ");     
    PrintRegion(pScreen, &pWinPriv->dirtyReg); 
    ErrorF("\n");							
    */

    pWinPriv->dirty = TRUE;						
    pScrPriv->windowsAreDirty = TRUE;					
}

static void
rwoutDamageDestroy (DamagePtr pDam, void *closure)
{
    RwoutDamagePtr pDamage = closure;
    
    pDamage->pDamage = 0;
    if (pDamage->id) {
	FreeResource (pDamage->id, RT_NONE);
    }
}

static Bool
rwoutCreateWindowWrite (ScreenPtr pScreen, WindowPtr pWin)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[CREATE_WINDOW_MESSAGE_SIZE];
    unsigned char decorated;
    short bw;
    int wid;
    short x, y;
    int wAndBorder, hAndBorder;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    decorated = (unsigned char) !pWin->overrideRedirect;
    bw = pWin->borderWidth;
    wid = pWin->drawable.id;
    x = pWin->drawable.x;
    y = pWin->drawable.y;
    wAndBorder = pWin->drawable.width + 2 * pWin->borderWidth;
    hAndBorder = pWin->drawable.height + 2 * pWin->borderWidth;

#ifdef VERBOSE
    ErrorF("*** CreateWindow, id = %d, decor = %d, xy %d, %d, whAndBorder = %d, %d, bw = %d\n", 
	   wid, decorated, x, y, wAndBorder, hAndBorder, bw);
#endif /* VERBOSE */

    swaps(&bw, n);
    swapl(&wid, n);
    swaps(&x, n);
    swaps(&y, n);
    swapl(&wAndBorder, n);
    swapl(&hAndBorder, n);

    CREATE_WINDOW_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_CREATE_WINDOW);
    CREATE_WINDOW_MESSAGE_SET_DECORATED(buf, decorated);
    CREATE_WINDOW_MESSAGE_SET_BORDER_WIDTH(buf, bw);
    CREATE_WINDOW_MESSAGE_SET_WID(buf, wid);
    CREATE_WINDOW_MESSAGE_SET_X(buf, x);
    CREATE_WINDOW_MESSAGE_SET_Y(buf, y);
    CREATE_WINDOW_MESSAGE_SET_WANDBORDER(buf, wAndBorder); 
    CREATE_WINDOW_MESSAGE_SET_HANDBORDER(buf, hAndBorder);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, CREATE_WINDOW_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutDestroyWindowWrite (ScreenPtr pScreen, WindowPtr pWin)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[DESTROY_WINDOW_MESSAGE_SIZE];
    int wid;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    wid = pWin->drawable.id;

#ifdef VERBOSE
    ErrorF("*** DestroyWindow, id = %d\n", wid);
#endif /* VERBOSE */

    swapl(&wid, n);

    DESTROY_WINDOW_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_DESTROY_WINDOW);
    DESTROY_WINDOW_MESSAGE_SET_WID(buf, wid);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, DESTROY_WINDOW_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutShowWindowWrite (ScreenPtr pScreen, WindowPtr pWin, int show)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[SHOW_WINDOW_MESSAGE_SIZE];
    int wid;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    wid = pWin->drawable.id;

#ifdef VERBOSE
    ErrorF("*** ShowWindow, id = %d, show = %d\n", wid, show);
#endif /* VERBOSE */

    swapl(&wid, n);

    SHOW_WINDOW_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_SHOW_WINDOW);
    SHOW_WINDOW_MESSAGE_SET_SHOW(buf, (unsigned char)show);
    SHOW_WINDOW_MESSAGE_SET_WID(buf, wid);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, SHOW_WINDOW_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutConfigureWindowWrite (ScreenPtr pScreen, WindowPtr pWin, int x, int y, 
			   unsigned int wAndBorder, unsigned int hAndBorder,
			   WindowPtr pSib)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[CONFIGURE_WINDOW_MESSAGE_SIZE];
    int clientId;
    int wid;
    short xval, yval;
    int wandb, handb;
    int sibid;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    
    clientId = pScrPriv->configuringClient;
    wid = pWin->drawable.id;
    xval = x;
    yval = y;
    wandb = wAndBorder;
    handb = hAndBorder;
    sibid = INVALID;
    if (pSib != NULL) {
	sibid = pSib->drawable.id;
    }    

#ifdef VERBOSE
    ErrorF("*** ConfigureWindow, clientId = %d, wid = %d, xy = %d, %d, whAndBorder = %d, %d",
	   clientId, wid, xval, yval, wandb, handb);
    if (pSib != NULL) {
	ErrorF(", pSib id = %d", sibid);
    }
    ErrorF("\n");
#endif /* VERBOSE */

    swapl(&clientId, n);
    swapl(&wid, n);
    swaps(&xval, n);
    swaps(&yval, n);
    swapl(&wandb, n);
    swapl(&handb, n);
    swapl(&sibid, n);

    CONFIGURE_WINDOW_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_CONFIGURE_WINDOW);
    CONFIGURE_WINDOW_MESSAGE_SET_CLIENT_ID(buf, clientId);
    CONFIGURE_WINDOW_MESSAGE_SET_WID(buf, wid);
    CONFIGURE_WINDOW_MESSAGE_SET_X(buf, xval);
    CONFIGURE_WINDOW_MESSAGE_SET_Y(buf, yval);
    CONFIGURE_WINDOW_MESSAGE_SET_WANDBORDER(buf, wandb); 
    CONFIGURE_WINDOW_MESSAGE_SET_HANDBORDER(buf, handb);
    CONFIGURE_WINDOW_MESSAGE_SET_SIBID(buf, sibid);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, CONFIGURE_WINDOW_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

Bool
rwoutPositionWindowWrite (ScreenPtr pScreen, WindowPtr pWin, int x, int y)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[POSITION_WINDOW_MESSAGE_SIZE];
    int clientId;
    int wid;
    short xval, yval;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    clientId = pScrPriv->configuringClient;
    wid = pWin->drawable.id;
    xval = x;
    yval = y;

#ifdef VERBOSE
    ErrorF("*** PositionWindow, clientId = %d, wid = %d, xy = %d, %d\n",  
	   clientId, wid, xval, yval);
#endif /* VERBOSE */

    swapl(&clientId, n);
    swapl(&wid, n);
    swaps(&xval, n);
    swaps(&yval, n);

    POSITION_WINDOW_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_POSITION_WINDOW);
    POSITION_WINDOW_MESSAGE_SET_CLIENT_ID(buf, clientid);
    POSITION_WINDOW_MESSAGE_SET_WID(buf, wid);
    POSITION_WINDOW_MESSAGE_SET_X(buf, xval);
    POSITION_WINDOW_MESSAGE_SET_Y(buf, yval);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, POSITION_WINDOW_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutWindowSetDecoratedWrite(ScreenPtr pScreen, WindowPtr pWin)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[WINDOW_SET_DECORATED_MESSAGE_SIZE];
    unsigned char decorated;
    int wid;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    decorated = (unsigned char) !pWin->overrideRedirect;
    wid = pWin->drawable.id;

#ifdef VERBOSE
    ErrorF("*** WindowSetDecorated, id = %d, decorated = %d\n",  
           wid, decorated);
#endif /* VERBOSE */

    swapl(&wid, n);

    WINDOW_SET_DECORATED_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_WINDOW_SET_DECORATED);
    WINDOW_SET_DECORATED_MESSAGE_SET_DECORATED(buf, decorated);
    WINDOW_SET_DECORATED_MESSAGE_SET_WID(buf, wid);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, WINDOW_SET_DECORATED_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutWindowSetBorderWidthWrite(ScreenPtr pScreen, WindowPtr pWin)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[WINDOW_SET_DECORATED_MESSAGE_SIZE];
    short bw;
    int wid;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    bw = pWin->borderWidth;
    wid = pWin->drawable.id;

#ifdef VERBOSE
    ErrorF("*** WindowSetBorderWidth, id = %d, bw = %d\n",  wid, bw);
#endif /* VERBOSE */

    swapl(&wid, n);
    swaps(&bw, n);

    WINDOW_SET_BORDER_WIDTH_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_WINDOW_SET_BORDER_WIDTH);
    WINDOW_SET_BORDER_WIDTH_MESSAGE_SET_BORDER_WIDTH(buf, bw);
    WINDOW_SET_BORDER_WIDTH_MESSAGE_SET_WID(buf, wid);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, WINDOW_SET_BORDER_WIDTH_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

/*
** Search up the window tree (starting at the given window's parent)
** until we find a window that is managed. Return NULL if no managed
** window is found.
*/

static WindowPtr
findManagedWindow (WindowPtr pWin) 
{
    pWin = pWin->parent;

    while (pWin != NULL) {
	if (MANAGED_WIN(pWin)) {
	    return pWin;
	}
	pWin = pWin->parent;
    }

    return NULL;
}

static Bool
rwoutCopyAreaWrite (WindowPtr pWin, int srcx, int srcy, int width, int height,
		    int dstx, int dsty)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[COPY_AREA_MESSAGE_SIZE];
    int sx, sy;
    int w, h;
    int dx, dy;
    int wid;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    wid = pWin->drawable.id;
    sx = srcx;
    sy = srcy;
    w = width;
    h = height;
    dx = dstx;
    dy = dsty;

#ifdef VERBOSE
    ErrorF("*** CopyArea id = %d, srcxy = %d, %d, wh = %d, %d, dstxy = %d, %d\n",  
	   wid, sx, sy, w, h, dx, dy);
#endif /* VERBOSE */

    swapl(&wid, n);
    swapl(&sx, n);
    swapl(&sy, n);
    swapl(&w, n);
    swapl(&h, n);
    swapl(&dx, n);
    swapl(&dy, n);

    COPY_AREA_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_COPY_AREA);
    COPY_AREA_MESSAGE_SET_WID(buf, wid);
    COPY_AREA_MESSAGE_SET_SRCX(buf, sx);
    COPY_AREA_MESSAGE_SET_SRCY(buf, sy);
    COPY_AREA_MESSAGE_SET_WIDTH(buf, w);
    COPY_AREA_MESSAGE_SET_HEIGHT(buf, h);
    COPY_AREA_MESSAGE_SET_DSTX(buf, dx);
    COPY_AREA_MESSAGE_SET_DSTY(buf, dy);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, COPY_AREA_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;

}

static Bool
rwoutUncodedPixelsWrite (ScreenPtr pScreen, WindowPtr pWin, int x, int y, int w, int h)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[DIRTY_AREA_RECT_SIZE];
    DirtyAreaRectPtr pDirty = (DirtyAreaRectPtr) &buf[0];
    unsigned char *pPixels;
    int n, i;
    /* Debug 
    int linesSent = 0;
    */

    /* Size of chunk (in bytes) */
    int chunkSize;  

    /* Size of chunk (in scan lines) */
    int chunkHeight;  

    /* Number of full chunks that need to be sent */
    int numChunks;
    
#ifdef VERBOSE
    ErrorF("uncoded rxywh = %d, %d, %d, %d\n", x, y, w, h);
#endif /* VERBOSE */

    /* First, send the information describing the dirty area */

    pDirty->x = x;
    pDirty->y = y;
    pDirty->width = w;
    pDirty->height = h;
    pDirty->encodingType = DISPLAY_PIXELS_ENCODING_UNCODED;

    swaps(&pDirty->x, n);
    swaps(&pDirty->y, n);
    swaps(&pDirty->width, n);
    swaps(&pDirty->height, n);
    swapl(&pDirty->encodingType, n);

    /* Send it off */
    if (!RWCOMM_BUFFER_WRITE(pComm, buf, DIRTY_AREA_RECT_SIZE)) {    
	return FALSE;
    }

    /* Lazy allocation of pixel buffer*/
    if (pScrPriv->pPixelBuf == NULL) {
	pScrPriv->pPixelBuf = (unsigned char *) 
	    xalloc(PIXEL_BUF_MAX_NUM_BYTES);
    }
    pPixels = pScrPriv->pPixelBuf;

    chunkHeight = PIXEL_BUF_MAX_NUM_PIXELS / w;
    numChunks = h / chunkHeight;
    chunkSize = w * chunkHeight * 4;
    /*
    ErrorF("chunkHeight = %d\n", chunkHeight);
    ErrorF("numChunks (full) = %d\n", numChunks);
    ErrorF("chunkSize = %d\n", chunkSize);
    */

    for (i = 0; i < numChunks; i++, h -= chunkHeight, y += chunkHeight) {

	/*
	ErrorF("Send full chunk %d: xywh = %d, %d, %d, %d\n",
	       i+1, x, y, w, chunkHeight);
	*/

	/* Fetch chunk from frame buffer */
	(*pScreen->GetImage)((DrawablePtr)pWin, x, y, w, chunkHeight, 
			     ZPixmap, ~0, (char *)pPixels);

	/* Send chunk */
	if (!RWCOMM_BUFFER_WRITE(pComm, (char *)pPixels, chunkSize)) { 
	    return FALSE;
	}

	/* Debug 
	linesSent += chunkHeight;
	*/
    }

    /* Send partial chunk at the end  if necessary */
    if (h > 0) {
	/*
	ErrorF("Send partial chunk xywh = %d, %d, %d, %d\n",
	       x, y, w, h);
	*/
	chunkSize = w * h * 4;
	(*pScreen->GetImage)((DrawablePtr)pWin, x, y, w, h, 
			     ZPixmap, ~0, (char *)pPixels);
	/*bytesSent += chunkSize;*/
	if (!RWCOMM_BUFFER_WRITE(pComm, (char *)pPixels, chunkSize)) { 
	    return FALSE;
	}

	/* Debug 
	linesSent += h;
	*/
    }

    /*
    ErrorF("linesSent = %d\n", linesSent);
    */

    return TRUE;
}

static Bool
rwoutWindowPixelsWrite (ScreenPtr pScreen, WindowPtr pWin, RemwinWindowPrivPtr pWinPriv)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[DISPLAY_PIXELS_MESSAGE_SIZE];
    RegionPtr pRegDirty = &pWinPriv->dirtyReg;
    int numDirty, numDirtyRects;
    int dx = pWin->drawable.x;
    int dy = pWin->drawable.y;
    BoxPtr pBox;
    int wid;
    int i, n;

    /* If window is no longer mapped, skip the messages */
    if (!pWin->mapped) {
	return TRUE;
    }

    /* 
    ** Make dirty region screen absolute and constrain it to 
    ** lie within the window (Not sure if this is strictly
    ** necessary but we'll do it for safety).
    */
    REGION_TRANSLATE(pScreen, pRegDirty, dx, dy);
    REGION_INTERSECT(pScreen, pRegDirty, pRegDirty, &pWin->borderSize);
    REGION_TRANSLATE(pScreen, pRegDirty, -dx, -dy);

    /* Bail out early if nothing is left */
    if (!REGION_NOTEMPTY(pScreen, pRegDirty)) {
	REGION_NULL(pScreen, pRegDirty);
	return TRUE;
    }

    /*
    ** Send the message header
    */

    numDirty = REGION_NUM_RECTS(pRegDirty);
    numDirtyRects = numDirty;
    wid = pWin->drawable.id;

#ifdef VERBOSE
    ErrorF("Send dirty pixels for wid = %d, numDirty = %d\n", 
	   wid, numDirty);
#endif /* VERBOSE */

    swaps(&numDirty, n);
    swapl(&wid, n);

    DISPLAY_PIXELS_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_DISPLAY_PIXELS);
    DISPLAY_PIXELS_MESSAGE_SET_NUM_DIRTY(buf, numDirty);
    DISPLAY_PIXELS_MESSAGE_SET_WID(buf, wid);

    /* Send it off */
    if (!RWCOMM_BUFFER_WRITE(pComm, buf, DISPLAY_PIXELS_MESSAGE_SIZE)) {    
	return FALSE;
    }

    pBox = REGION_RECTS(pRegDirty);
    for (i = 0; i < numDirtyRects; i++, pBox++) {
	if (useRle24) {
	    if (!rlePixelsWrite(pScreen, pWin, pBox->x1, pBox->y1, 
				pBox->x2 - pBox->x1, pBox->y2 - pBox->y1)) {
		REGION_UNINIT(pScreen, pRegDirty);
		return FALSE;
	    }
	    if (rwRleVerifyPixels) {
		if (!rwoutUncodedPixelsWrite(pScreen, pWin, pBox->x1, pBox->y1, 
		     pBox->x2 - pBox->x1, pBox->y2 - pBox->y1)) {
		    REGION_UNINIT(pScreen, pRegDirty);
		    return FALSE;
		}
	    }
	} else {
	    if (!rwoutUncodedPixelsWrite(pScreen, pWin, pBox->x1, pBox->y1, 
		   pBox->x2 - pBox->x1, pBox->y2 - pBox->y1)) {
		REGION_UNINIT(pScreen, pRegDirty);
		return FALSE;
	    }
	}
    }

    REGION_NULL(pScreen, pRegDirty);

    return TRUE;
}

static Bool
rwoutPixelsWrite (ScreenPtr pScreen)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    WindowPtr pWin;
    RemwinWindowPrivPtr pWinPriv;

    pWin = pScrPriv->pManagedWindows;
    while (pWin != NULL) {
	pWinPriv = REMWIN_GET_WINPRIV(pWin);

	if (pWinPriv->dirty) {
	    if (!rwoutWindowPixelsWrite(pScreen, pWin, pWinPriv)) {
		return FALSE;
	    }
	}
	
	pWinPriv->dirty = !REGION_NOTEMPTY(pWin->pScreen, &pWinPriv->dirtyReg);
	pScrPriv->windowsAreDirty = pScrPriv->windowsAreDirty || pWinPriv->dirty;

	pWin = pWinPriv->pWinNext;
    }

    return TRUE;
}

/* Same as xfixes/cursor.c:GetBit */
static int
GetBit (unsigned char *line, int x)
{
    unsigned char   mask;
    
    if (screenInfo.bitmapBitOrder == LSBFirst)
	mask = (1 << (x & 7));
    else
	mask = (0x80 >> (x & 7));
    /* XXX assumes byte order is host byte order */
    line += (x >> 3);
    if (*line & mask)
	return 1;
    return 0;
}

/* Same as xfixes/cursor.c:CopyCursorToImage */
static void
CopyCursorToImage (CursorPtr pCursor, CARD32 *image)
{
    int width = pCursor->bits->width;
    int height = pCursor->bits->height;
    int npixels = width * height;
    
#ifdef ARGB_CURSOR
    if (pCursor->bits->argb)
	memcpy (image, pCursor->bits->argb, npixels * sizeof (CARD32));
    else
#endif
    {
	unsigned char	*srcLine = pCursor->bits->source;
	unsigned char	*mskLine = pCursor->bits->mask;
	int		stride = BitmapBytePad (width);
	int		x, y;
	CARD32		fg, bg;
	
	fg = (0xff000000 | 
	      ((pCursor->foreRed & 0xff00) << 8) |
	      (pCursor->foreGreen & 0xff00) |
	      (pCursor->foreBlue >> 8));
	bg = (0xff000000 | 
	      ((pCursor->backRed & 0xff00) << 8) |
	      (pCursor->backGreen & 0xff00) |
	      (pCursor->backBlue >> 8));
	for (y = 0; y < height; y++)
	{
	    for (x = 0; x < width; x++)
	    {
		if (GetBit (mskLine, x))
		{
		    if (GetBit (srcLine, x))
			*image++ = fg;
		    else
			*image++ = bg;
		}
		else
		    *image++ = 0;
	    }
	    srcLine += stride;
	    mskLine += stride;
	}
    }
}

static Bool
rwoutDisplayCursorWrite (ScreenPtr pScreen, CursorPtr pCursor)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[DISPLAY_CURSOR_MESSAGE_SIZE];
    short w, h;
    short xh, yh;
    int npixels;
    CARD32 *pImage;
    int imageSize;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    /*
    ** First write message header
    */

    w = pCursor->bits->width;
    h = pCursor->bits->height;
    xh = pCursor->bits->xhot;
    yh = pCursor->bits->yhot;

#ifdef VERBOSE
    ErrorF("*** DisplayCursor wh = %d, %d, xyhot = %d, %d\n",  
	   w, h, xh, yh);
#endif /* VERBOSE */

    swaps(&w, n);
    swaps(&h, n);
    swaps(&xh, n);
    swaps(&yh, n);
    
    DISPLAY_CURSOR_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_DISPLAY_CURSOR);
    DISPLAY_CURSOR_MESSAGE_SET_WIDTH(buf, w);
    DISPLAY_CURSOR_MESSAGE_SET_HEIGHT(buf, h);
    DISPLAY_CURSOR_MESSAGE_SET_XHOT(buf, xh);
    DISPLAY_CURSOR_MESSAGE_SET_YHOT(buf, yh);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, DISPLAY_CURSOR_MESSAGE_SIZE)) {    
	return FALSE;
    }

    /*
    ** Then convert the cursor to a 32-bit pixel image and send.
    */
    
    w = pCursor->bits->width;
    h = pCursor->bits->height;
    npixels = w * h;
    imageSize = npixels * sizeof (CARD32);

    pImage = xalloc(imageSize);
    if (pImage == NULL) {
	return FALSE;
    }

    CopyCursorToImage(pCursor, pImage);

    if (!RWCOMM_BUFFER_WRITE(pComm, (char *)pImage, imageSize)) {    
	return FALSE;
    }

    xfree(pImage);

    return TRUE;
}

static Bool
rwoutMoveCursorWrite (ScreenPtr pScreen, WindowPtr pWin, int x, int y)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[MOVE_CURSOR_MESSAGE_SIZE];
    int wid;
    int cx, cy;
    int n;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

    wid = pWin->drawable.id;
    cx = x;
    cy = y;

#ifdef VERBOSE
    ErrorF("*** MoveCursor: wid = %d, xy = %d, %d\n",  wid, cx, cy);
#endif /* VERBOSE */

    swapl(&wid, n);
    swapl(&cx, n);
    swapl(&cy, n);
    
    MOVE_CURSOR_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_MOVE_CURSOR);
    MOVE_CURSOR_MESSAGE_SET_WID(buf, wid);
    MOVE_CURSOR_MESSAGE_SET_X(buf, cx);
    MOVE_CURSOR_MESSAGE_SET_Y(buf, cy);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, MOVE_CURSOR_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutShowCursorWrite (ScreenPtr pScreen, Bool show)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[SHOW_CURSOR_MESSAGE_SIZE];
    char showit = show ? 1 : 0;

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

#ifdef VERBOSE
    ErrorF("*** ShowCursor: show = %d\n", showit);
#endif /* VERBOSE */
    
    SHOW_CURSOR_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_SHOW_CURSOR);
    SHOW_CURSOR_MESSAGE_SET_SHOW(buf, showit);

    if (!RWCOMM_BUFFER_WRITE(pComm, buf, SHOW_CURSOR_MESSAGE_SIZE)) {    
	return FALSE;
    }

    return TRUE;
}

Bool 
rwoutSetWindowTitlesWrite (ScreenPtr pScreen, int strLen, char *buf)
{
    /* TODO: this causes sol new to fail */
#ifdef NOTYET
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char hdrBuf[SET_WINDOW_TITLES_MESSAGE_SIZE];

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return TRUE;
    }

#ifdef VERBOSE
    ErrorF("*** SetWindowTitles: strLen = %d\n", strLen);
#endif /* VERBOSE */
    
    /* First send the length */
    SET_WINDOW_TITLES_MESSAGE_SET_TYPE(hdrBuf, SERVER_MESSAGE_TYPE_SET_WINDOW_TITLES);
    SET_WINDOW_TITLES_MESSAGE_SET_STRLEN(hdrBuf, strLen);
    if (!RWCOMM_BUFFER_WRITE(pComm, hdrBuf, SET_WINDOW_TITLES_MESSAGE_SIZE)) {    
	return FALSE;
    }

    /* Then send the string */
    if (!RWCOMM_BUFFER_WRITE(pComm, buf, strLen)) {    
	return FALSE;
    }
#endif /* NOTYET */

    return TRUE;
}

static Bool
rwoutCreateWindow(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RemwinWindowPrivPtr pWinPriv = REMWIN_GET_WINPRIV(pWin);
    RwoutDamagePtr pDamage;
    Bool ret;

    PROLOG(pScrPriv, CreateWindow);
    ret = (*pScreen->CreateWindow)(pWin);
    EPILOG(CreateWindow, rwoutCreateWindow);
    if (!ret) {
	return FALSE;
    }

    if (!MANAGED_WIN(pWin)) {
	return TRUE;
    }

    pDamage = xalloc(sizeof(RwoutDamageRec));
    if (pDamage == NULL) {
	return BadAlloc;
    }
    pDamage->id = pWin->drawable.id;
    pDamage->pDrawable = (DrawablePtr) pWin;
    pDamage->level = DamageReportRawRegion;
    pDamage->pDamage = DamageCreate (rwoutDamageReport, rwoutDamageDestroy, pDamage->level,
				     FALSE, pWin->drawable.pScreen, pDamage);
    if (!pDamage->pDamage)
    {
	xfree (pDamage);
	return FALSE;;
    }
    if (!AddResource (pDamage->id, DamageExtType, (pointer) pDamage)) {
	xfree (pDamage);
	return FALSE;
    }

    DamageRegister(pDamage->pDrawable, pDamage->pDamage);

    /* Initial damage must include border */
    RegionPtr pRegion = &pWin->borderClip;
    DamageDamageRegion((DrawablePtr)pWin, pRegion);

    if (!rwoutCreateWindowWrite(pScreen, pWin)) {
	ErrorF("WARNING: could not notify clients of window creation for window %d\n", 
	       (int)pWin->drawable.id);
	return FALSE;
    }

    /* TODO: DELETE
    pWinPriv = (RemwinWindowPrivPtr) xalloc(sizeof(RemwinWindowPrivRec));
    if (pWinPriv == NULL) {
	return FALSE;
    }
    REMWIN_SET_WINPRIV(pWin, pWinPriv);
    */

    REGION_NULL(pScreen, &pWinPriv->dirtyReg);
    pWinPriv->mapped = FALSE;

    /* Add to front of window list */
    pWinPriv->pWinNext = pScrPriv->pManagedWindows;
    pScrPriv->pManagedWindows = pWin;

    return TRUE;
}

static Bool
rwoutDestroyWindow(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RemwinWindowPrivPtr pWinPriv = REMWIN_GET_WINPRIV(pWin);
    WindowPtr pW, pWPrev;
    Bool ret;

    PROLOG(pScrPriv, DestroyWindow);
    ret = (*pScreen->DestroyWindow)(pWin);
    EPILOG(DestroyWindow, rwoutDestroyWindow);
    if (!ret) {
	return FALSE;
    }

    if (!MANAGED_WIN(pWin) ||
	pWinPriv == NULL) {
	return TRUE;
    }

    /* Unlink from window list */
    pWPrev = NULL;
    pW = pScrPriv->pManagedWindows;
    while (pW != NULL && pW != pWin) {
	RemwinWindowPrivPtr pWPriv = REMWIN_GET_WINPRIV(pW);
	pWPrev = pW;
	pW = pWPriv->pWinNext;
    }
    if (pW != pWin) {
	ErrorF("WARNING: cannot find window %d among managed windows\n", 
	       (int)pWin->drawable.id);
	EPILOG(DestroyWindow, rwoutDestroyWindow);
	return FALSE;
    }
    pWinPriv = REMWIN_GET_WINPRIV(pW);
    if (pWPrev == NULL) {
	pScrPriv->pManagedWindows = pWinPriv->pWinNext;
    } else {
	RemwinWindowPrivPtr pWinPrivPrev = REMWIN_GET_WINPRIV(pWPrev)
	pWinPrivPrev->pWinNext = pWinPriv->pWinNext;
    }

    REGION_UNINIT(pScreen, &pWinPriv->dirtyReg);

    if (!rwoutDestroyWindowWrite(pScreen, pWin)) {
	ErrorF("WARNING: could not notify clients of window destruction for window %d\n",
	       (int)pWin->drawable.id);
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutRealizeWindow(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    Bool ret;
    RemwinWindowPrivPtr pWinPriv = REMWIN_GET_WINPRIV(pWin);

    PROLOG(pScrPriv, RealizeWindow);
    ret = (*pScreen->RealizeWindow)(pWin);
    EPILOG(RealizeWindow, rwoutRealizeWindow);
    if (!ret) {
	return FALSE;
    }

    if (!MANAGED_WIN(pWin)) {
	return TRUE;
    }

    pWinPriv->mapped = TRUE;

    if (!rwoutShowWindowWrite(pScreen, pWin, 1)) {
	ErrorF("WARNING: could not notify clients of window mapping for window %d\n",
	       (int)pWin->drawable.id);
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutUnrealizeWindow(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    Bool ret;
    RemwinWindowPrivPtr pWinPriv = REMWIN_GET_WINPRIV(pWin);

    PROLOG(pScrPriv, UnrealizeWindow);
    ret = (*pScreen->UnrealizeWindow)(pWin);
    EPILOG(UnrealizeWindow, rwoutUnrealizeWindow);
    if (!ret) {
	return FALSE;
    }

    if (!MANAGED_WIN(pWin)) {
	return TRUE;
    }

    pWinPriv->mapped = FALSE;

    /* The client no longer needs the pixels of this window. Clear the dirty regions */
    pWinPriv->dirty = FALSE;
    REGION_UNINIT(pScreen, &pWinPriv->dirtyReg);

    if (!rwoutShowWindowWrite(pScreen, pWin, 0)) {
	ErrorF("WARNING: could not notify clients of window unmapping for window %d\n",
	       (int)pWin->drawable.id);
	return FALSE;
    }

    return TRUE;
}

static void
rwoutResizeWindow(WindowPtr pWin, int x, int y, unsigned int w, unsigned int h, WindowPtr pSib)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    PROLOG(pScrPriv, ResizeWindow);
    (*pScreen->ResizeWindow)(pWin, x, y, w, h, pSib);
    EPILOG(ResizeWindow, rwoutResizeWindow);

    if (!MANAGED_WIN(pWin)) {
	return;
    }

    if (!rwoutConfigureWindowWrite(pScreen, pWin, x, y, 
				w + 2 * pWin->borderWidth, h + 2 * pWin->borderWidth, 
				pSib)) {
	ErrorF("WARNING: could not notify clients of window move/resize/restack for window %d\n",
	       (int)pWin->drawable.id);
    }
}

static Bool
rwoutPositionWindow (WindowPtr pWin, int x, int y)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    Bool ret;

    PROLOG(pScrPriv, PositionWindow);
    ret = (*pScreen->PositionWindow)(pWin, x, y);
    EPILOG(PositionWindow, rwoutPositionWindow);
    if (!ret) {
	return FALSE;
    }

    if (!MANAGED_WIN(pWin)) {
	return TRUE;
    }

    if (!rwoutPositionWindowWrite(pScreen, pWin, x, y)) {
	ErrorF("WARNING: could not notify clients of window move for window %d\n",
	       (int)pWin->drawable.id);
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutChangeWindowAttributes (WindowPtr pWin, Mask vmask)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    Bool ret;

    PROLOG(pScrPriv, ChangeWindowAttributes);
    ret = (*pScreen->ChangeWindowAttributes)(pWin, vmask);
    EPILOG(ChangeWindowAttributes, rwoutChangeWindowAttributes);
    if (!ret) {
	return FALSE;
    }

    /* TODO: not yet supporting input only windows */
    if (pWin->drawable.type == UNDRAWABLE_WINDOW ||
	!MANAGED_WIN(pWin)) {
	return TRUE;
    }

    if ((vmask & CWOverrideRedirect) != 0 &&
	!rwoutWindowSetDecoratedWrite(pScreen, pWin)) {
        ErrorF("WARNING: could not notify clients of decorated change for window %d\n",
	       (int)pWin->drawable.id);
	return FALSE;
    }

    if ((vmask & CWBorderWidth) != 0 &&
	!rwoutWindowSetBorderWidthWrite(pScreen, pWin)) {
        ErrorF("WARNING: could not notify clients of border width change for window %d\n",
	       (int)pWin->drawable.id);
	return FALSE;
    }

    return TRUE;
}

void
rwoutBeep (int percent, DeviceIntPtr pDevice,
	   pointer ctrl, int unused) 
{
    /* TODO: for now */
    ScreenPtr pScreen = screenInfo.screens[0];

    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    /* TODO: notyet 
    char buf[BEEP_MESSAGE_SIZE];
    */

    if (!RWCOMM_IS_CONNECTED(pComm)) {
	return;
    }

#ifdef VERBOSE
    ErrorF("*** Beep\n");
#endif /* VERBOSE */

    /* TODO: bug this causes gt to hang
    POSITION_WINDOW_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_BEEP);

    (void) RWCOMM_BUFFER_WRITE(pComm, buf, BEEP_MESSAGE_SIZE);
    */
}

static Bool
rwoutDisplayCursor (ScreenPtr pScreen, CursorPtr pCursor)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    Bool ret;

    PROLOG(pScrPriv, DisplayCursor);
    ret = (*pScreen->DisplayCursor)(pScreen, pCursor);
    EPILOG(DisplayCursor, rwoutDisplayCursor);
    if (!ret) {
	return FALSE;
    }

    if (pCursor == pScrPriv->pCursor) {
	return TRUE;
    }

    pScrPriv->pCursor = pCursor;

    if (!rwoutDisplayCursorWrite(pScreen, pCursor)) {
	ErrorF("WARNING: could not notify clients of new cursor\n");
	return FALSE;
    }

    return TRUE;
}

static Bool
rwoutSetCursorPosition (ScreenPtr pScreen, int x, int y, Bool generateEvent)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    WindowPtr pWin;
    Bool ret;

    PROLOG(pScrPriv, SetCursorPosition);
    ret = (*pScreen->SetCursorPosition)(pScreen, x, y, generateEvent);
    EPILOG(SetCursorPosition, rwoutSetCursorPosition);
    if (!ret) {
	return FALSE;
    }

    /* Determine whether cursor is in a managed window */
    pWin = dixGetSpriteWindow();
    if (!MANAGED_WIN(pWin)) {
	pWin = findManagedWindow(pWin);
    }
    if (pWin == NULL) {

	/* If window isn't in a manage window then disable cursor */
	pScrPriv->pCursorWin = NULL;
	if (!rwoutShowCursorWrite(pScreen, FALSE)) {
	    ErrorF("WARNING: could not notify clients of cursor not visible\n");
	}
	return TRUE;
    }
	
    /* Calculate window relative cursor position */
    x -= pWin->drawable.x;
    y -= pWin->drawable.y;

    /* Show cursor if it is not currently shown */
    if (pScrPriv->pCursorWin == NULL) {
	if (!rwoutShowCursorWrite(pScreen, TRUE)) {
	    ErrorF("WARNING: could not notify clients of cursor made visible\n");
	    return FALSE;
	}
    }

    pScrPriv->pCursorWin = pWin;
    pScrPriv->cursorX = x;
    pScrPriv->cursorY = y;

    if (!rwoutMoveCursorWrite(pScreen, pWin, x, y)) {
        ErrorF("WARNING: could not notify clients of cursor move in win %d\n",
	       (int)pWin->drawable.id);
	return FALSE;
    }

    return TRUE;
}

static void
rwoutTimerReset (ScreenPtr pScreen)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    TimerSet(pScrPriv->outputTimer, 0, TIMER_MS, rwoutTimerCallback, pScreen);
}

static CARD32
rwoutTimerCallback (OsTimerPtr timer, CARD32 now, pointer arg)
{
    ScreenPtr pScreen = (ScreenPtr) arg;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    if (pScrPriv->windowsAreDirty) {
	rwoutPixelsWrite(pScreen);
	pScrPriv->windowsAreDirty = FALSE;
    }

    /* Reenable for the next tick */
    rwoutTimerReset(pScreen);
    
    return 0;
}

void
rwoutTimerCreate (ScreenPtr pScreen) 
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    pScrPriv->outputTimer = NULL;
    rwoutTimerReset(pScreen);
}

void
rwoutTimerDestroy (ScreenPtr pScreen)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    TimerFree(pScrPriv->outputTimer);
    pScrPriv->outputTimer = NULL;
}

static Bool
rwoutCreateGC (GCPtr pGC)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pGC->pScreen);
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);
    ScreenPtr pScreen = pGC->pScreen;
    Bool ret;

    bzero(pGCPriv, sizeof(RemwinGCPrivRec));

    PROLOG(pScrPriv, CreateGC);

    if ((ret = (*pScreen->CreateGC)(pGC))) {
	FUNC_EPILOG(pGC, pGCPriv);
    }

    EPILOG(CreateGC, rwoutCreateGC);

    return ret;
}

static void
rwoutValidateGC(GCPtr pGC, unsigned long stateChanges, DrawablePtr pDrawable)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    FUNC_PROLOG(pGC, pGCPriv);

    (*pGC->funcs->ValidateGC)(pGC, stateChanges, pDrawable);

    FUNC_EPILOG(pGC, pGCPriv);
}

static void
rwoutChangeGC (GCPtr pGC, unsigned long mask)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    FUNC_PROLOG(pGC, pGCPriv);

    (*pGC->funcs->ChangeGC) (pGC, mask);

    FUNC_EPILOG(pGC, pGCPriv);
}

static void
rwoutCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGCDst);

    FUNC_PROLOG(pGCDst, pGCPriv);

    (*pGCDst->funcs->CopyGC) (pGCSrc, mask, pGCDst);

    FUNC_EPILOG(pGCDst, pGCPriv);
}

static void
rwoutDestroyGC(GCPtr pGC)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    FUNC_PROLOG(pGC, pGCPriv);

    (*pGC->funcs->DestroyGC) (pGC);

    /* leave it unwrapped */
}

static void
rwoutChangeClip(GCPtr pGC, int type, pointer pvalue, int nrects)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    FUNC_PROLOG(pGC, pGCPriv);

    (*pGC->funcs->ChangeClip)(pGC, type, pvalue, nrects);

    FUNC_EPILOG(pGC, pGCPriv);
}

static void
rwoutCopyClip(GCPtr pgcDst, GCPtr pgcSrc)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pgcDst);

    FUNC_PROLOG(pgcDst, pGCPriv);

    (*pgcDst->funcs->CopyClip)(pgcDst, pgcSrc);

    FUNC_EPILOG(pgcDst, pGCPriv);
}

static void
rwoutDestroyClip(GCPtr pGC)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    FUNC_PROLOG(pGC, pGCPriv);

    (*pGC->funcs->DestroyClip)(pGC);

    FUNC_EPILOG(pGC, pGCPriv);
}

static void
rwoutFillSpans(DrawablePtr pDst, GCPtr pGC, int nspans, DDXPointPtr ppt,
	       int *pwidth, int fSorted)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->FillSpans)(pDst, pGC, nspans, ppt, pwidth, fSorted);

    OPS_EPILOG(pGC);
}

static void
rwoutSetSpans(DrawablePtr pDst, GCPtr pGC, char *psrc, DDXPointPtr ppt,
	      int *pwidth, int nspans, int fSorted)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->SetSpans)(pDst, pGC, psrc, ppt, pwidth, nspans, fSorted);

    OPS_EPILOG(pGC);
}

static void
rwoutPutImage(DrawablePtr pDst, GCPtr pGC, int depth, int x, int y, int w, int h,
	      int leftPad, int format, char *pBits)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PutImage)(pDst, pGC, depth, x, y, w, h, leftPad, format, pBits);

    OPS_EPILOG(pGC);
}

static RegionPtr
rwoutCopyArea(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC, int srcx, int srcy,
	   int w, int h, int dstx, int dsty)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);
    RegionPtr exposed;
    Bool clientCopy = FALSE;
    WindowPtr pWin;

    /* 
    ** Only do client-side copies when the destination is a managed window,
    ** the src is the same as the dest, and the optimization is enabled.
    */
    if (clientSideCopies &&
	pDst == pSrc &&
	pDst->type == DRAWABLE_WINDOW) {
	clientCopy = TRUE;
	ignoreCopyAreaDamage = TRUE;
	ErrorF("Use client side copy\n");
    }
	
    OPS_PROLOG(pGC);
    exposed = (*pGC->ops->CopyArea)(pSrc, pDst, pGC, srcx, srcy, w, h,
				    dstx, dsty);
    OPS_EPILOG(pGC);
    ignoreCopyAreaDamage = FALSE;

    if (!clientCopy) {
	return exposed;
    }

    /* Find the corresponding managed window */
    pWin = findManagedWindow((WindowPtr)pDst);
    if (pWin != (WindowPtr)pDst) {
	/* Make the rectangles relative to the managed window */
	int xOffset = pDst->x - pWin->drawable.x;
	int yOffset = pDst->y - pWin->drawable.y;
	srcx += xOffset;
	srcy += yOffset;
	dstx += xOffset;
	dsty += yOffset;
    }

    if (!rwoutCopyAreaWrite((WindowPtr) pDst, srcx, srcy, w, h, dstx, dsty)) {
	ErrorF("WARNING: could not notify clients of copy area for window %d\n",
	       (int)pDst->id);
	return FALSE;
    }

    return exposed;
}

static RegionPtr
rwoutCopyPlane(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC, int srcx, int srcy,
	       int w, int h, int dstx, int dsty, unsigned long plane)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);
    RegionPtr exposed;

    OPS_PROLOG(pGC);

    exposed = (*pGC->ops->CopyPlane)(pSrc, pDst, pGC, srcx, srcy, w, h,
				     dstx, dsty, plane);

    OPS_EPILOG(pGC);

    return exposed;
}

static void
rwoutPolyPoint(DrawablePtr pDst, GCPtr pGC, int mode, int npt, xPoint *ppt)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PolyPoint)(pDst, pGC, mode, npt, ppt);

    OPS_EPILOG(pGC);
}

static void
rwoutPolylines(DrawablePtr pDst, GCPtr pGC, int mode, int npt, DDXPointPtr ppt)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->Polylines)(pDst, pGC, mode, npt, ppt);

    OPS_EPILOG(pGC);
}

static void
rwoutPolySegment(DrawablePtr pDst, GCPtr pGC, int nseg, xSegment *pSegs)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PolySegment)(pDst, pGC, nseg, pSegs);

    OPS_EPILOG(pGC);
}

static void
rwoutPolyRectangle(DrawablePtr pDst, GCPtr pGC, int nrects, xRectangle *pRects)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PolyRectangle)(pDst, pGC, nrects, pRects);

    OPS_EPILOG(pGC);
}

static void
rwoutPolyArc(DrawablePtr pDst, GCPtr pGC, int narcs, xArc *pArcs)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PolyArc)(pDst, pGC, narcs, pArcs);

    OPS_EPILOG(pGC);
}

static void
rwoutFillPolygon(DrawablePtr pDst, GCPtr pGC, int shape, int mode, int npt,
		 DDXPointPtr ppt)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->FillPolygon)(pDst, pGC, shape, mode, npt, ppt);

    OPS_EPILOG(pGC);
}

static void
rwoutPolyFillRect(DrawablePtr pDst, GCPtr pGC, int nrects, xRectangle *pRects)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PolyFillRect)(pDst, pGC, nrects, pRects);

    OPS_EPILOG(pGC);
}

static void
rwoutPolyFillArc(DrawablePtr pDst, GCPtr pGC, int narcs, xArc *parcs)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PolyFillArc)(pDst, pGC, narcs, parcs);

    OPS_EPILOG(pGC);
}

static int
rwoutPolyText8(DrawablePtr pDst, GCPtr pGC, int x, int y, int count, char *chars)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);
    int result;

    OPS_PROLOG(pGC);

    result = (*pGC->ops->PolyText8)(pDst, pGC, x, y, count, chars);

    OPS_EPILOG(pGC);

    return result;
}

static int
rwoutPolyText16(DrawablePtr pDst, GCPtr pGC, int x, int y, int count,
	     unsigned short *chars)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);
    int result;

    OPS_PROLOG(pGC);

    result = (*pGC->ops->PolyText16)(pDst, pGC, x, y, count, chars);

    OPS_EPILOG(pGC);

    return result;
}

static void
rwoutImageText8(DrawablePtr pDst, GCPtr pGC, int x, int y, int count, char *chars)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->ImageText8)(pDst, pGC, x, y, count, chars);

    OPS_EPILOG(pGC);
}

static void
rwoutImageText16(DrawablePtr pDst, GCPtr pGC, int x, int y, int count,
		 unsigned short *chars)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->ImageText16)(pDst, pGC, x, y, count, chars);

    OPS_EPILOG(pGC);
}

static void
rwoutImageGlyphBlt(DrawablePtr pDst, GCPtr pGC, int x, int y, unsigned int nglyph,
		   CharInfoPtr *ppci, pointer pglyphBase)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->ImageGlyphBlt)(pDst, pGC, x, y, nglyph, ppci, pglyphBase);

    OPS_EPILOG(pGC);
}

static void
rwoutPolyGlyphBlt(DrawablePtr pDst, GCPtr pGC, int x, int y, unsigned int nglyph,
	       CharInfoPtr *ppci, pointer pglyphBase)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PolyGlyphBlt)(pDst, pGC, x, y, nglyph, ppci, pglyphBase);

    OPS_EPILOG(pGC);
}

static void
rwoutPushPixels(GCPtr pGC, PixmapPtr pBitMap, DrawablePtr pDst, int w, int h,
		int x, int y)
{
    RemwinGCPrivPtr pGCPriv = REMWIN_GET_GCPRIV(pGC);

    OPS_PROLOG(pGC);

    (*pGC->ops->PushPixels)(pGC, pBitMap, pDst, w, h, x, y);

    OPS_EPILOG(pGC);
}

    
static void
rwoutSyncWindowOnConnect (ScreenPtr pScreen, RwcommClientPtr pCommClient,
			  WindowPtr pWin) 
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    char buf[DISPLAY_PIXELS_MESSAGE_SIZE];
    RegionPtr pReg = &pWin->borderSize;
    int numRectsToSend, numRects;
    BoxPtr pBox;
    int wid;
    int i, n;

    if (!rwoutCreateWindowWrite(pScreen, pWin)) {
	ErrorF("WARNING: could not notify newly connected client of existence of managed window\n");
	return;
    }

    /* Send a configure in order to update the sibling information (the other info is redundant) */
    if (!rwoutConfigureWindowWrite(pScreen, pWin, pWin->drawable.x, pWin->drawable.y, 
	        pWin->drawable.width + 2 * pWin->borderWidth, 
		pWin->drawable.height + 2 * pWin->borderWidth, 
		pWin->nextSib)) {
	ErrorF("WARNING: could not notify newly connected client of managed window configuration\n");
    }

    /* Send the current show state */
    if (!rwoutShowWindowWrite(pScreen, pWin, pWin->mapped ? 1 : 0)) {
	ErrorF("WARNING: could not notify clients of window mapping for window %d\n",
	       (int)pWin->drawable.id);
	return;
    }
    
    if (!pWin->mapped) {
	return;
    }

    /* Prepare the DisplayPixels message header */
    numRects = REGION_NUM_RECTS(pReg);
    numRectsToSend = numRects;
    wid = pWin->drawable.id;

#ifdef VERBOSE
    ErrorF("Send sync pixels for wid = %d, numRect = %d\n", 
	   wid, numRects);
#endif /* VERBOSE */

    swaps(&numRectsToSend, n);
    swapl(&wid, n);

    DISPLAY_PIXELS_MESSAGE_SET_TYPE(buf, SERVER_MESSAGE_TYPE_DISPLAY_PIXELS);
    DISPLAY_PIXELS_MESSAGE_SET_NUM_DIRTY(buf, numRectsToSend);
    DISPLAY_PIXELS_MESSAGE_SET_WID(buf, wid);

    /* Send it off */
    if (!RWCOMM_BUFFER_WRITE(pComm, buf, DISPLAY_PIXELS_MESSAGE_SIZE)) {    
	return;
    }

    pBox = REGION_RECTS(pReg);
    for (i = 0; i < numRects; i++, pBox++) {
	if (!rwoutUncodedPixelsWrite(pScreen, pWin, pBox->x1, pBox->y1, 
				     pBox->x2 - pBox->x1, pBox->y2 - pBox->y1)) {
	    return;
	}
    }
}

static void
rwoutSyncCursorOnConnect (ScreenPtr pScreen, RwcommClientPtr pCommClient)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);

    /* Only non-null if cursor is in a managed window */
    if (pScrPriv->pCursorWin == NULL) {
	return;
    }

    /* Send cursor image and hot spot */
    if (!rwoutDisplayCursorWrite(pScreen, pScrPriv->pCursor)) {
	ErrorF("WARNING: could not sync cursor image for newly connected client\n");
	return;
    }
    
    /* Send current cursor */
    if (!rwoutMoveCursorWrite(pScreen, pScrPriv->pCursorWin, 
			      pScrPriv->cursorX, pScrPriv->cursorY)) {
        ErrorF("WARNING: could not sync cursor position for newly connected client\n");
	return;
    }

    /* Since cursor is in a managed window, show it */
    if (!rwoutShowCursorWrite(pScreen, TRUE)) {
	ErrorF("WARNING: could not sync cursor show for newly connected client\n");
	return;
    }
}

void
rwoutSyncClientOnConnect (ScreenPtr pScreen, RwcommClientPtr pCommClient) 
{
    WindowPtr pRootWin = WindowTable[pScreen->myNum];
    WindowPtr pChild = pRootWin->firstChild;

    /* Send information about managed windows */
    while (pChild) {
	/* TODO: we aren't yet supporting input only windows */
	if (pChild->drawable.type == DRAWABLE_WINDOW) {
	    rwoutSyncWindowOnConnect(pScreen, pCommClient, pChild);
	}
	pChild = pChild->nextSib;
    }

    /* Send information about cursor */
    rwoutSyncCursorOnConnect(pScreen, pCommClient);
}
