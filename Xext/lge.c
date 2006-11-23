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

#ifdef LG3D

#define NEED_REPLIES
#define NEED_EVENTS
#include <unistd.h>
#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "dixstruct.h"
#include "extnsionst.h"
#include "windowstr.h"
#include "scrnintstr.h"
#include "cursorstr.h"
#include "inputstr.h"
#include "lgewire.h"
#include "lgeint.h"

int lgeDisplayServerIsAlive = 0;

ClientPtr lgePickerClient = NULL;
ClientPtr lgeEventDelivererClient = NULL;
int lgeEventComesFromDS = 0;

/*
** If sendEventDirect == true we will call the core process event 
** routine directly. The event will not be subject to queue freezing. 
** If sendEventDirect == false we will reinsert the event via the 
** device's processInputProc routine. This means that the event will
** be subject to queue freezing. That is to say, if the queue is currently
** frozen due to a synchronous grab the event will be enqueued and not
** sent further up the event pipeline until the queue is unfrozen.
*/

Bool lgeSendEventDirect = FALSE;

DeviceIntPtr lgekb = NULL;
DeviceIntPtr lgems = NULL;

/* The pseudo-root window of first screen */
Window lgeDisplayServerPRW = INVALID;
WindowPtr pLgeDisplayServerPRWWin = NULL;

Window lgeDisplayServerPRWsList[MAXSCREENS];
WindowPtr pLgeDisplayServerPRWWinsList[MAXSCREENS];
WindowPtr pLgeDisplayServerPRWWinRoots[MAXSCREENS];

static int numRegisteredScr = 0;

static int  LgeReqCode;

void LgeExtensionInit (void);
int LgeShutdownDisplayServer (pointer data, XID id);
int ProcLgeDispatch (ClientPtr client);
int SProcLgeDispatch (ClientPtr client);

static RESTYPE lgeClientResourceType;

#ifdef PERF
/* For performance analysis */
#include "../dix/statbuf.h"
static StatBuf *sbPerf = NULL;
static struct timeval tvStart, tvStop;
#endif /* PERF */

extern void XkbProcessKeyboardEvent(xEvent *xE, DeviceIntPtr keybd, int count);

extern WindowPtr *WindowTable;

static void lgeDisableIncompatibleExtensions ();
static void lgeReenableIncompatibleExtensions ();

/* This must be the same as in dix/extension.c */
#define EXTENSION_BASE  128

extern unsigned int NumExtensions;
extern ExtensionEntry **extensions;

static int
ProcLgeQueryVersion (ClientPtr client)
{
    REQUEST(xLgeQueryVersionReq);
    xLgeQueryVersionReply rep;
    int n;

    REQUEST_SIZE_MATCH(xLgeQueryVersionReq);
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.majorVersion = LGE_MAJOR_VERSION;
    rep.minorVersion = LGE_MINOR_VERSION;
    rep.implementation = LG3D_IMPLEMENTATION;

    if (client->swapped) {
	swaps(&rep.sequenceNumber, n);
	swapl(&rep.majorVersion, n);
	swapl(&rep.minorVersion, n);
	swapl(&rep.implementation, n);
    }
    WriteToClient(client, sizeof(xLgeQueryVersionReply), (char *) &rep);
    return (client->noClientException);
}

/* For debug

static void
printEventMask (unsigned int mask) 
{
    int i;
    unsigned int bit;

    for (i = 0, bit = 0x1; i < 32; i++, bit <<= 1) {
	if ((mask & bit) != 0) {
	    switch (mask & bit) {
	    case KeyPressMask: 
		ErrorF("KeyPressMask ");
		break;
	    case KeyReleaseMask: 
		ErrorF("KeyReleaseMask ");
		break;
	    case ButtonPressMask: 
		ErrorF("ButtonPressMask ");
		break;
	    case ButtonReleaseMask: 
		ErrorF("ButtonReleaseMask ");
		break;
	    case EnterWindowMask: 
		ErrorF("EnterWindowMask ");
		break;
	    case LeaveWindowMask: 
		ErrorF("LeaveWindowMask ");
		break;
	    case PointerMotionMask: 
		ErrorF("PointerMotionMask ");
		break;
	    case PointerMotionHintMask: 
		ErrorF("PointerMotionHintMask ");
		break;
	    case Button1MotionMask: 
		ErrorF("Button1MotionMask ");
		break;
	    case Button2MotionMask: 
		ErrorF("Button2MotionMask ");
		break;
	    case Button3MotionMask: 
		ErrorF("Button3MotionMask ");
		break;
	    case Button4MotionMask: 
		ErrorF("Button4MotionMask ");
		break;
	    case Button5MotionMask: 
		ErrorF("Button5MotionMask ");
		break;
	    case ButtonMotionMask: 
		ErrorF("ButtonMotionMask ");
		break;
	    case KeymapStateMask: 
		ErrorF("KeymapStateMask ");
		break;
	    case ExposureMask: 
		ErrorF("ExposureMask ");
		break;
	    case VisibilityChangeMask: 
		ErrorF("VisibilityChangeMask ");
		break;
	    case StructureNotifyMask: 
		ErrorF("StructureNotifyMask ");
		break;
	    case ResizeRedirectMask: 
		ErrorF("ResizeRedirectMask ");
		break;
	    case SubstructureNotifyMask: 
		ErrorF("SubstructureNotifyMask ");
		break;
	    case SubstructureRedirectMask: 
		ErrorF("SubstructureRedirectMask ");
		break;
	    case FocusChangeMask: 
		ErrorF("FocusChangeMask ");
		break;
	    case PropertyChangeMask: 
		ErrorF("PropertyChangeMask ");
		break;
	    case ColormapChangeMask: 
		ErrorF("ColormapChangeMask ");
		break;
	    case OwnerGrabButtonMask: 
		ErrorF("OwnerGrabButtonMask ");
		break;
	    }
	    ErrorF("\n");
	}
    }
}

static void
printEventInterest (WindowPtr pWin) 
{
    OtherClients * others;

    ErrorF("Owner client = %d\n", wClient(pWin)->index);
    ErrorF("Client event interest for prw %d\n", pWin->drawable.id);
    ErrorF("Owner interest: eventMask = 0x%x\n", pWin->eventMask);
    printEventMask(pWin->eventMask);
    ErrorF("\n");
    ErrorF("Non-owner interest:\n");
    for (others = wOtherClients(pWin); others; others = others->next) {
	ErrorF("Other client mask = 0x%x\n", others->mask);
	printEventMask(others->mask);
	ErrorF("\n");
    }
}
*/

static int
ProcLgeRegisterClient (ClientPtr client)
{
    REQUEST(xLgeRegisterClientReq);

    REQUEST_SIZE_MATCH(xLgeRegisterClientReq);
  
    switch (stuff->clientType) {

    case LGE_CLIENT_PICKER:
	if (lgePickerClient != NULL) {
	    return BadAccess;
	}
	lgePickerClient = client;
        lgeSendEventDirect = stuff->sendEventDirect;
	break;

    case LGE_CLIENT_EVENT_DELIVERER:
	if (lgeEventDelivererClient != NULL) {
	    return BadAccess;
	}
	lgeEventDelivererClient = client;
	break;
    }

    /* The first client to close down tells the server that the DS has shut down */
    if (!AddResource (FakeClientID(client->index), lgeClientResourceType, NULL)) {
	return BadAlloc;
    }

    return client->noClientException;
}

static int
ProcLgeRegisterScreen (ClientPtr client)
{
    REQUEST(xLgeRegisterScreenReq);
    ScreenPtr pScreen;
    WindowPtr pWin;
    int       scrNum;

    REQUEST_SIZE_MATCH(xLgeRegisterScreenReq);
  
    /* Only the picker client can make this request */
    if (client != lgePickerClient) {
	return BadAccess;
    }

    pWin = (WindowPtr) LookupIDByType (stuff->prw, RT_WINDOW);
    if (pWin == NULL) {
	return BadMatch;
    }
    scrNum = pWin->drawable.pScreen->myNum;
    lgeDisplayServerPRWsList[scrNum] = stuff->prw;
    pLgeDisplayServerPRWWinsList[scrNum] = pWin;
    pLgeDisplayServerPRWWinRoots[scrNum] = WindowTable[scrNum];

    if (numRegisteredScr == 0) {
	lgeDisplayServerPRW = stuff->prw;
	pLgeDisplayServerPRWWin = pWin;
    } 
    else if (numRegisteredScr >= MAXSCREENS) {
    	return BadAlloc;
    }

    pScreen = pWin->drawable.pScreen;
    numRegisteredScr++;

    /* Prevent events from being sent to the PRW owner, which is AWT */
    pWin->eventMask = 0;

#ifdef PERF
    /* For performance analysis */
    sbPerf = statBufCreate();
    if (sbPerf == NULL) {
	return BadAlloc;
    }
#endif /* PERF */

    return client->noClientException;
}

static int
ProcLgeControlLgMode (ClientPtr client)
{
    REQUEST (xLgeControlLgModeReq);

    REQUEST_SIZE_MATCH(xLgeControlLgModeReq);

    /* Only the picker client can make this request */
    if (client != lgePickerClient) {
	return BadAccess;
    }

    if (stuff->enable == TRUE) {
	lgeDisplayServerIsAlive = 1;
	lgeDisableIncompatibleExtensions();
    } else {
	lgeReenableIncompatibleExtensions();
	LgeShutdownDisplayServer(NULL, INVALID);
    }

    return client->noClientException;
}

static int
ProcLgeSendEvent (ClientPtr client)
{
    REQUEST (xLgeSendEventReq);
    xEvent *xE = (xEvent *)&stuff->event;

    REQUEST_SIZE_MATCH(xLgeSendEventReq);

    /* Only the picker client can make this request */
    if (client != lgePickerClient) {
	return BadAccess;
    }

    /*
    ErrorF("ProcLgeSendEvent, event type = %d, pickSeq = %d\n", 
	   xE->u.u.type, xE->u.u.sequenceNumber);
    ErrorF("detail = %d\n", xE->u.u.detail);
    ErrorF("time   = %d\n", xE->u.keyButtonPointer.time);
    ErrorF("root   = %d\n", xE->u.keyButtonPointer.root);
    ErrorF("event   = %d\n", xE->u.keyButtonPointer.event);
    ErrorF("child   = %d\n", xE->u.keyButtonPointer.child);
    ErrorF("rootX   = %d\n", xE->u.keyButtonPointer.rootX);
    ErrorF("rootY   = %d\n", xE->u.keyButtonPointer.rootY);
    ErrorF("eventX   = %d\n", xE->u.keyButtonPointer.eventX);
    ErrorF("eventY   = %d\n", xE->u.keyButtonPointer.eventY);
    ErrorF("state   = %d\n", xE->u.keyButtonPointer.state);
    */

    lgeEventComesFromDS = 1;
    if (xE->u.u.type == KeyPress || xE->u.u.type == KeyRelease) {

	if (lgeSendEventDirect) {
	    XkbProcessKeyboardEvent(xE, inputInfo.keyboard, 1);
	} else {
	    (*inputInfo.keyboard->public.processInputProc)(xE, inputInfo.keyboard, 1);
	}

    } else {

#ifdef PERF
	/* For performance analysis */
	float usStart, usStop, msDelta;
	gettimeofday(&tvStart, 0);
#endif /* PERF */

	if (lgeSendEventDirect) {
	    CoreProcessPointerEvent(xE, inputInfo.pointer, 1);
	} else {
	    (*inputInfo.pointer->public.processInputProc)(xE, inputInfo.pointer, 1);
	}

#ifdef PERF
	/* For performance analysis */
	gettimeofday(&tvStop, 0);
	usStart = 1000000.0f * tvStart.tv_sec + tvStart.tv_usec;
	usStop = 1000000.0f * tvStop.tv_sec + tvStop.tv_usec;
	msDelta = (usStop - usStart) / 1000.0f;
	statBufAdd(sbPerf, msDelta);
#endif /* PERF */
    }
    lgeEventComesFromDS = 0;

    return client->noClientException;
}

static int
SProcLgeQueryVersion (ClientPtr client)
{
    int n;
    REQUEST(xLgeQueryVersionReq);

    swaps(&stuff->length, n);
    REQUEST_SIZE_MATCH(xLgeQueryVersionReq);

    return ProcLgeQueryVersion(client);
}

static int
SProcLgeRegisterClient (ClientPtr client)
{
    int n;
    REQUEST(xLgeRegisterClientReq);

    swaps(&stuff->length, n);
    REQUEST_SIZE_MATCH(xLgeRegisterClientReq);
    return ProcLgeRegisterClient(client);
}

static int
SProcLgeRegisterScreen (ClientPtr client)
{
    int n;
    REQUEST(xLgeRegisterScreenReq);

    swaps(&stuff->length, n);
    REQUEST_SIZE_MATCH(xLgeRegisterScreenReq);
    swapl(&stuff->prw, n);
    return ProcLgeRegisterScreen(client);
}

static int
SProcLgeControlLgMode (ClientPtr client)
{
    int n;
    REQUEST(xLgeControlLgModeReq);

    swaps(&stuff->length, n);
    REQUEST_SIZE_MATCH(xLgeControlLgModeReq);
    return ProcLgeControlLgMode(client);
}

static int
SProcLgeSendEvent (ClientPtr client)
{
    int n;
    EventSwapPtr proc;
    xEvent eventT;

    REQUEST(xLgeSendEventReq);
    swaps (&stuff->length, n);
    REQUEST_AT_LEAST_SIZE (xLgeSendEventReq);

    /* Swap event */
    proc = EventSwapVector[stuff->event.u.u.type & 0177];
    if (!proc ||  proc == NotImplemented)    /* no swapping proc; invalid event type? */
       return (BadValue);
    (*proc)(&stuff->event, &eventT);
    stuff->event = eventT;

    return ProcLgeSendEvent (client);
}

int
ProcLgeDispatch (ClientPtr client)
{
  REQUEST(xReq);

  switch (stuff->data) {
  case X_LgeQueryVersion:
      return ProcLgeQueryVersion(client);
  case X_LgeRegisterClient:
      return ProcLgeRegisterClient(client);
  case X_LgeRegisterScreen:
      return ProcLgeRegisterScreen(client);
  case X_LgeControlLgMode:
      return ProcLgeControlLgMode(client);
  case X_LgeSendEvent:
      return ProcLgeSendEvent(client);
  default:
      return BadRequest;
  }
}

int
SProcLgeDispatch (ClientPtr client)
{
  REQUEST(xReq);

  switch (stuff->data) {

  case X_LgeQueryVersion:
      return SProcLgeQueryVersion(client);

  case X_LgeRegisterClient: 
      return SProcLgeRegisterClient(client);

  case X_LgeRegisterScreen: 
      return SProcLgeRegisterScreen(client);

  case X_LgeControlLgMode: 
      return SProcLgeControlLgMode(client);

  case X_LgeSendEvent: 
      return SProcLgeSendEvent(client);

  default:
      return BadRequest;
  }
}

/*ARGSUSED*/
int
LgeShutdownDisplayServer (pointer data, XID id)
{
    int i;

    if (!lgeDisplayServerIsAlive) {
	return 1;
    }

    /*ErrorF("LG Display Server has shut down\n");*/

    lgeDisplayServerIsAlive = 0;
    lgeEventComesFromDS = 0;
    lgekb = NULL;
    lgems = NULL;

    lgePickerClient = NULL;
    lgeEventDelivererClient = NULL;

    lgeDisplayServerPRW = INVALID;
    pLgeDisplayServerPRWWin = NULL;

    for (i = 0; i < MAXSCREENS; i++) {
	lgeDisplayServerPRWsList[i] = INVALID;
	pLgeDisplayServerPRWWinsList[i] = NULL;
	pLgeDisplayServerPRWWinRoots[i] = NULL;
    }
    numRegisteredScr = 0;

    return 1;
}

static void
LgeReset (ExtensionEntry *extEntry)
{
    (void) LgeShutdownDisplayServer(NULL, INVALID);
}

void
LgeExtensionInit (void)
{
  ExtensionEntry *extEntry;
  int            i;

  lgeClientResourceType = CreateNewResourceType(LgeShutdownDisplayServer);
  if (lgeClientResourceType == 0) {
      FatalError("LgeExtensionInit: Cannot create display server resource type\n");
      exit(1);
  }
    
  extEntry = AddExtension(LGE_NAME, 0, 0, ProcLgeDispatch,
			  SProcLgeDispatch, LgeReset, StandardMinorOpcode);
  if (extEntry == NULL) {
      FatalError("LgeExtensionInit: AddExtension(%s) failed\n", LGE_NAME);
      exit(1);
  }
 
  LgeReqCode = extEntry->base;

  for (i = 0; i < MAXSCREENS; i++) {
      lgeDisplayServerPRWsList[i] = INVALID;
      pLgeDisplayServerPRWWinsList[i] = NULL;
      pLgeDisplayServerPRWWinRoots[i] = NULL;
  }
}          

int
IsWinLgePRWOne(int win) 
{
    int i;
	
    for (i=0; i < numRegisteredScr; i++)
    {
	if(lgeDisplayServerPRWsList[i] == win)
            return win;
    }	
    return INVALID;
}

WindowPtr
GetLgePRWWinFor(int win) 
{
    int i;
	
    for (i=0; i < numRegisteredScr; i++)
    {
	if(lgeDisplayServerPRWsList[i] == win)
	     return pLgeDisplayServerPRWWinsList[i];			
    }	
    return NULL;
}

/*
** Returns True if the given extension is compatible with LG mode.
** Returns false if the given extension is incompatible.
** Specifically, XVideo is incompatible because it not redirectable 
** by the composite extension. And XINPUT is incompatible because it 
** is not yet supported by the LG-modified Xorg event subsystem.
** 
** TODO: GLX and NV-GLX are not compatible with LG either. But 
** disabling these extensions has no effect; OpenGL apps run anyway
** event without these extensions defined. I have not yet found
** a way to gracefully keep OpenGL programs from running while
** LG mode is abled.
*/

Bool
lgeCompatibleExtension (ExtensionEntry *pExt)
{
    if (strcmp(pExt->name, "XVideo") == 0) {
	return FALSE;
    }
    if (strcmp(pExt->name, "XVideo-MotionCompensation") == 0) {
	return FALSE;
    }

    if (strcmp(pExt->name, "XInputExtension") == 0) {
	return FALSE;
    }

    return TRUE;
}

/*
** Shuts the given extension off by replacing its MainProc and SwappedMainProc
** with a routine which prints out an error message and returns BadAccess,
** or enables the given extension by replacing its original MainProc and 
** SwappedMainProc routines.
*/

#define WARNING_MESSAGE "WARNING: client attempt to access the %s extension, which has been disabled because it is incompatible with LG3D"

static int (*XVideoSavedMainProc)(ClientPtr);
static int (*XVideoSavedSwappedMainProc)(ClientPtr);
static int (*XVideoMCSavedMainProc)(ClientPtr);
static int (*XVideoMCSavedSwappedMainProc)(ClientPtr);
static int (*XINPUTSavedMainProc)(ClientPtr);
static int (*XINPUTSavedSwappedMainProc)(ClientPtr);

static int
lgeXVideoError (ClientPtr client) 
{
    ErrorF(WARNING_MESSAGE, "XVideo");
    return BadAccess;
}

static int
lgeXVideoMCError (ClientPtr client) 
{
    ErrorF(WARNING_MESSAGE, "XVideo-MotionCompensation");
    return BadAccess;
}

static int
lgeXINPUTError (ClientPtr client) 
{
    ErrorF(WARNING_MESSAGE, "XInputExtension");
    return BadAccess;
}

static void
lgeControlIncompatibleExtension (ExtensionEntry *pExt, Bool enable)
{
    int idx = pExt->index;

    if (strcmp(pExt->name, "XVideo") == 0) {
	if (enable) {
	    ProcVector[idx + EXTENSION_BASE] = XVideoSavedMainProc;
	    SwappedProcVector[idx + EXTENSION_BASE] = XVideoSavedSwappedMainProc;
	} else {
	    XVideoSavedMainProc = ProcVector[idx + EXTENSION_BASE];
	    ProcVector[idx + EXTENSION_BASE] = lgeXVideoError;
	    XVideoSavedSwappedMainProc = SwappedProcVector[idx + EXTENSION_BASE];
	    SwappedProcVector[idx + EXTENSION_BASE] = lgeXVideoError;
	}
    }
    if (strcmp(pExt->name, "XVideo-MotionCompensation") == 0) {
	if (enable) {
	    ProcVector[idx + EXTENSION_BASE] = XVideoMCSavedMainProc;
	    SwappedProcVector[idx + EXTENSION_BASE] = XVideoMCSavedSwappedMainProc;
	} else {
	    XVideoMCSavedMainProc = ProcVector[idx + EXTENSION_BASE];
	    ProcVector[idx + EXTENSION_BASE] = lgeXVideoMCError;
	    XVideoMCSavedSwappedMainProc = SwappedProcVector[idx + EXTENSION_BASE];
	    SwappedProcVector[idx + EXTENSION_BASE] = lgeXVideoMCError;
	}
    }

    /* 
     * In order to fix LG bug 665 we must not disable XINPUT here, because
     * realplay requires it. But we will still not advertise the extension
     * in the list of supported extensions.
     *
     * This seems to be a linux-only issue. So right now were only 
     * reenabling it on linux.

    if (strcmp(pExt->name, "XInputExtension") == 0) {
	if (enable) {
	    ProcVector[idx + EXTENSION_BASE] = XINPUTSavedMainProc;
	    SwappedProcVector[idx + EXTENSION_BASE] = XINPUTSavedSwappedMainProc;
	} else {
	    XINPUTSavedMainProc = ProcVector[idx + EXTENSION_BASE];
	    ProcVector[idx + EXTENSION_BASE] = lgeXINPUTError;
	    XINPUTSavedSwappedMainProc = SwappedProcVector[idx + EXTENSION_BASE];
	    SwappedProcVector[idx + EXTENSION_BASE] = lgeXINPUTError;
	}	
    }
    */
}

static void
lgeDisableIncompatibleExtensions ()
{
    int i;
    
    for (i = 0;  i < NumExtensions; i++) {
	if (!lgeCompatibleExtension(extensions[i])) {
	    lgeControlIncompatibleExtension(extensions[i], FALSE);
	}
    }
}

static void
lgeReenableIncompatibleExtensions ()
{
    int i;
    
    for (i = 0;  i < NumExtensions; i++) {
	if (!lgeCompatibleExtension(extensions[i])) {
	    lgeControlIncompatibleExtension(extensions[i], TRUE);
	}
    }
}

#endif /* LG3D */


