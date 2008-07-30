/*
 * Copyright 2001-2003 Red Hat Inc., Durham, North Carolina.
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
 *   David H. Dawes <dawes@xfree86.org>
 *   Kevin E. Martin <kem@redhat.com>
 *   Rickard E. (Rik) Faith <faith@redhat.com>
 */

/** \file
 * These routines support taking input from devices on the backend
 * (output) displays.  \see dmxcommon.c. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#define DMX_BACKEND_DEBUG 0

#include "dmxinputinit.h"
#include "dmxbackend.h"
#include "dmxcommon.h"
#include "dmxconsole.h"
#include "dmxcursor.h"
#include "dmxwindow.h"
#include "dmxprop.h"
#include "dmxsync.h"
#include "dmxxlibio.h"
#include "dmxcb.h"              /* For dmxGlobalWidth and dmxGlobalHeight */
#include "dmxevents.h"          /* For dmxGetGlobalPosition */
#include "ChkNotMaskEv.h"

#include "inputstr.h"
#include "input.h"
#include <X11/keysym.h>
#include "mi.h"
#include "mipointer.h"
#include "scrnintstr.h"
#include "windowstr.h"

static EventListPtr dmxEvents = NULL;

/* Private area for backend devices. */
typedef struct _myPrivate {
    DMX_COMMON_PRIVATE;
    int                     myScreen;
    DMXScreenInfo           *grabbedScreen;
    
    int                     lastX, lastY;
    int                     centerX, centerY;
    int                     relative;
    int                     newscreen;
    int                     initialized;
    DevicePtr               mou, kbd;
    int                     entered;
    int                     offX, offY;
} myPrivate;

#if DMX_BACKEND_DEBUG
#define DMXDBG0(f)                   dmxLog(dmxDebug,f)
#define DMXDBG1(f,a)                 dmxLog(dmxDebug,f,a)
#define DMXDBG2(f,a,b)               dmxLog(dmxDebug,f,a,b)
#define DMXDBG3(f,a,b,c)             dmxLog(dmxDebug,f,a,b,c)
#define DMXDBG4(f,a,b,c,d)           dmxLog(dmxDebug,f,a,b,c,d)
#define DMXDBG5(f,a,b,c,d,e)         dmxLog(dmxDebug,f,a,b,c,d,e)
#define DMXDBG6(f,a,b,c,d,e,g)       dmxLog(dmxDebug,f,a,b,c,d,e,g)
#define DMXDBG7(f,a,b,c,d,e,g,h)     dmxLog(dmxDebug,f,a,b,c,d,e,g,h)
#define DMXDBG8(f,a,b,c,d,e,g,h,i)   dmxLog(dmxDebug,f,a,b,c,d,e,g,h,i)
#define DMXDBG9(f,a,b,c,d,e,g,h,i,j) dmxLog(dmxDebug,f,a,b,c,d,e,g,h,i,j)
#else
#define DMXDBG0(f)
#define DMXDBG1(f,a)
#define DMXDBG2(f,a,b)
#define DMXDBG3(f,a,b,c)
#define DMXDBG4(f,a,b,c,d)
#define DMXDBG5(f,a,b,c,d,e)
#define DMXDBG6(f,a,b,c,d,e,g)
#define DMXDBG7(f,a,b,c,d,e,g,h)
#define DMXDBG8(f,a,b,c,d,e,g,h,i)
#define DMXDBG9(f,a,b,c,d,e,g,h,i,j)
#endif

/** Create and return a private data structure. */
pointer dmxBackendCreatePrivate(DeviceIntPtr pDevice)
{
    GETDMXLOCALFROMPDEVICE;
    myPrivate *priv = calloc(1, sizeof(*priv));
    priv->dmxLocal  = dmxLocal;
    return priv;
}

/** Destroy the private data structure.  No checking is performed to
 * verify that the structure was actually created by
 * #dmxBackendCreatePrivate. */
void dmxBackendDestroyPrivate(pointer private)
{
    if (private) free(private);
}

static void *dmxBackendTestScreen(DMXScreenInfo *dmxScreen, void *closure)
{
    long target = (long)closure;

    if (dmxScreen->index == target) return dmxScreen;
    return NULL;
}

/* Return non-zero if screen and priv->myScreen are on the same physical
 * backend display (1 if they are the same screen, 2 if they are
 * different screens).  Since this is a common operation, the results
 * are cached.  The cache is invalidated if \a priv is NULL (this should
 * be done with each server generation and reconfiguration). */
static int dmxBackendSameDisplay(myPrivate *priv, long screen)
{
    static myPrivate *oldpriv  = NULL;
    static int       oldscreen = -1;
    static int       retcode   = 0;

    if (priv == oldpriv && screen == oldscreen) return retcode;
    if (!priv) {                /* Invalidate cache */
        oldpriv   = NULL;
        oldscreen = -1;
        retcode   = 0;
        return 0;
    }

    if (screen == priv->myScreen)                     retcode = 1;
    else if (screen < 0 || screen >= dmxNumScreens)   retcode = 0;
    else if (dmxPropertyIterate(priv->be,
                                dmxBackendTestScreen,
                                (void *)screen))      retcode = 2;
    else                                              retcode = 0;

    oldpriv   = priv;
    oldscreen = screen;
    return retcode;
}

typedef struct _TestEventData {
    DMXInputInfo *dmxInput;
    XEvent       *X;
    long	 deviceId;
} TestEventData;

static Bool inputEventPredicate (Display  *xdisplay,
				 XEvent   *X,
				 XPointer arg)
{
    TestEventData *data = (TestEventData *) arg;
    int		  i;

    switch (X->type) {
    case MotionNotify:
    case ButtonPress:
    case ButtonRelease:
    case KeyPress:
    case KeyRelease:
    case KeymapNotify:
	data->deviceId = -1;
	*(data->X) = *X;
	return TRUE;
    default:
	for (i = 0; i < DMX_XINPUT_EVENT_NUM; i++)
	{
	    if (data->dmxInput->event[i] == X->type)
	    {
		switch (i) {
		case XI_DeviceMotionNotify: {
		    XDeviceMotionEvent *dmev = (XDeviceMotionEvent *) X;
		    XMotionEvent       *mev = (XMotionEvent *) data->X;

		    mev->type   = MotionNotify;
		    mev->x      = dmev->x;
		    mev->y      = dmev->y;
		    mev->state  = dmev->state;
		    mev->window = dmev->window;
		} break;
		case XI_DeviceButtonPress: {
		    XDeviceButtonEvent *dbev = (XDeviceButtonEvent *) X;
		    XButtonEvent       *bev = (XButtonEvent *) data->X;

		    bev->type   = ButtonPress;
		    bev->button = dbev->button;
		    bev->x      = dbev->x;
		    bev->y      = dbev->y;
		    bev->state  = dbev->state;
		    bev->window = dbev->window;
		} break;
		case XI_DeviceButtonRelease: {
		    XDeviceButtonEvent *dbev = (XDeviceButtonEvent *) X;
		    XButtonEvent       *bev = (XButtonEvent *) data->X;

		    bev->type   = ButtonRelease;
		    bev->button = dbev->button;
		    bev->x      = dbev->x;
		    bev->y      = dbev->y;
		    bev->state  = dbev->state;
		    bev->window = dbev->window;
		} break;
		case XI_DeviceKeyPress: {
		    XDeviceKeyEvent *dkev = (XDeviceKeyEvent *) X;
		    XKeyEvent       *kev = (XKeyEvent *) data->X;

		    kev->type    = KeyPress;
		    kev->keycode = dkev->keycode;
		    kev->x       = dkev->x;
		    kev->y       = dkev->y;
		    kev->state   = dkev->state;
		} break;
		case XI_DeviceKeyRelease: {
		    XDeviceKeyEvent *dkev = (XDeviceKeyEvent *) X;
		    XKeyEvent       *kev = (XKeyEvent *) data->X;

		    kev->type    = KeyRelease;
		    kev->keycode = dkev->keycode;
		    kev->x       = dkev->x;
		    kev->y       = dkev->y;
		    kev->state   = dkev->state;
		} break;
		default:
		    *(data->X) = *X;
		}

		data->deviceId = ((XDeviceStateNotifyEvent *) X)->deviceid;
		return TRUE;
	    }
	}
    }

    return FALSE;
}

static void *dmxBackendTestEvents(DMXScreenInfo *dmxScreen, void *closure)
{
    XEvent X;
    int    result = 0;
    
    XLIB_PROLOGUE (dmxScreen);
    result = XCheckIfEvent (dmxScreen->beDisplay,
			    &X,
			    inputEventPredicate,
			    closure);
    XLIB_EPILOGUE (dmxScreen);
    return (result) ? dmxScreen : NULL;
}

static void *dmxBackendTestMotionEvent(DMXScreenInfo *dmxScreen, void *closure)
{
    XEvent *X = (XEvent *)closure;
    int result = 0;

    XLIB_PROLOGUE (dmxScreen);
    result = XCheckTypedEvent(dmxScreen->beDisplay, MotionNotify, X);
    XLIB_EPILOGUE (dmxScreen);
    return (result) ? dmxScreen : NULL;
}

static DMXScreenInfo *dmxBackendGetEvent(myPrivate *priv, XEvent *X)
{
    DMXScreenInfo *dmxScreen;

    if ((dmxScreen = dmxPropertyIterate(priv->be, dmxBackendTestEvents, X)))
        return dmxScreen;
    return NULL;
}

static void *dmxBackendTestWindow(DMXScreenInfo *dmxScreen, void *closure)
{
    Window win = (Window)(long)closure;
    if (dmxScreen->scrnWin == win) return dmxScreen;
    return NULL;
}

void dmxBackendUpdatePosition(pointer private, int x, int y)
{
}

static DeviceIntPtr
dmxGetButtonDevice (DMXInputInfo *dmxInput,
		    XID          deviceId)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
	if (dmxInput->devs[i]->pDevice->button)
	    if (deviceId < 0 || dmxInput->devs[i]->deviceId == deviceId)
		return dmxInput->devs[i]->pDevice;

    return NULL;
}

static DeviceIntPtr
dmxGetPairedButtonDevice (DMXInputInfo *dmxInput,
			  DeviceIntPtr pDevice)
{
    GETDMXLOCALFROMPDEVICE;

    return dmxGetButtonDevice (dmxInput, dmxLocal->attached);
}

static DeviceIntPtr
dmxGetKeyboardDevice (DMXInputInfo *dmxInput,
		      XID          deviceId)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
	if (dmxInput->devs[i]->pDevice->key)
	    if (deviceId < 0 || dmxInput->devs[i]->deviceId == deviceId)
		return dmxInput->devs[i]->pDevice;

    return NULL;
}

static DeviceIntPtr
dmxGetPairedKeyboardDevice (DMXInputInfo *dmxInput,
			    DeviceIntPtr pDevice)
{
    GETDMXLOCALFROMPDEVICE;

    return dmxGetKeyboardDevice (dmxInput, dmxLocal->attached);
}

static int
dmxButtonEvent (DeviceIntPtr pDevice,
		int          button,
		int          x,
		int          y,
		int          type)
{
    ScreenPtr scr = miPointerGetScreen (pDevice);
    int       mx = pDevice->valuator->axes[0].max_value;
    int       my = pDevice->valuator->axes[1].max_value;
    int       v[2] = { x, y };
    int       nEvents, i;

    pDevice->valuator->axes[0].max_value = scr->width;
    pDevice->valuator->axes[1].max_value = scr->height;

    GetEventList (&dmxEvents);
    nEvents = GetPointerEvents (dmxEvents,
				pDevice,
				type,
				button,
				POINTER_ABSOLUTE,
				0, 2,
				v);

    pDevice->valuator->axes[0].max_value = mx;
    pDevice->valuator->axes[1].max_value = my;

    for (i = 0; i < nEvents; i++)
	mieqEnqueue (pDevice, dmxEvents[i].event);

    if (button > 0 && button <= 255)
    {
	GETDMXLOCALFROMPDEVICE;

	switch (type) {
	case ButtonPress:
	    dmxLocal->state[button >> 3] |= 1 << (button & 7);
	    break;
	case ButtonRelease:
	    dmxLocal->state[button >> 3] &= ~(1 << (button & 7));
	default:
	    break;
	}
    }
	
    return nEvents;
}

static int
dmxKeyEvent (DeviceIntPtr pDevice,
	     int          key,
	     int          type)
{
    int i, nEvents;

    GetEventList (&dmxEvents);
    nEvents = GetKeyboardEvents (dmxEvents, pDevice, type, key);
    for (i = 0; i < nEvents; i++)
	mieqEnqueue (pDevice, dmxEvents[i].event);

    if (key > 0 && key <= 255)
    {
	GETDMXLOCALFROMPDEVICE;

	switch (type) {
	case KeyPress:
	    dmxLocal->state[key >> 3] |= 1 << (key & 7);
	    break;
	case KeyRelease:
	    dmxLocal->state[key >> 3] &= ~(1 << (key & 7));
	default:
	    break;
	}
    }

    return nEvents;
}

static int
dmxUpdateButtonState (DeviceIntPtr pDevice,
		      const char   *buttons)
{
    int i, j, nEvents = 0;

    GETDMXLOCALFROMPDEVICE;

    for (i = 0; i < 32; i++)
    {
	if (!(dmxLocal->state[i] ^ buttons[i]))
	    continue;
	
	for (j = 0; j < 8; j++)
	{
	    /* button is down, but shouldn't be */
	    if ((dmxLocal->state[i] & (1 << j)) && !(buttons[i] & (1 << j)))
		nEvents += dmxButtonEvent (pDevice,
					   (i << 3) + j,
					   pDevice->last.valuators[0],
					   pDevice->last.valuators[1],
					   ButtonRelease);

	    /* button should be down, but isn't */
	    if (!(dmxLocal->state[i] & (1 << j)) && (buttons[i] & (1 << j)))
		nEvents += dmxButtonEvent (pDevice,
					   (i << 3) + j,
					   pDevice->last.valuators[0],
					   pDevice->last.valuators[1],
					   ButtonPress);
	}
    }

    return nEvents;
}

static int
dmxUpdateKeyState (DeviceIntPtr pDevice,
		   const char   *keys)
{
    int i, j, nEvents = 0;

    GETDMXLOCALFROMPDEVICE;

    for (i = 0; i < 32; i++)
    {
	if (!(dmxLocal->state[i] ^ keys[i]))
	    continue;

	for (j = 0; j < 8; j++)
	{
	    /* key is down, but shouldn't be */
	    if ((dmxLocal->state[i] & (1 << j)) && !(keys[i] & (1 << j)))
		nEvents += dmxKeyEvent (pDevice,
					(i << 3) + j,
					KeyRelease);

	    /* key should be down, but isn't */
	    if (!(dmxLocal->state[i] & (1 << j)) && (keys[i] & (1 << j)))
		nEvents += dmxKeyEvent (pDevice,
					(i << 3) + j,
					KeyPress);
	}
    }

    return nEvents;
}

static int
dmxChangeButtonState (DeviceIntPtr pDevice,
		      int          button,
		      int          how)
{
    int nEvents = 0;

    if (button > 0 && button <= 255)
    {
	char buttons[32];

	GETDMXLOCALFROMPDEVICE;

	memcpy (buttons, dmxLocal->state, sizeof (buttons));

	switch (how) {
	case ButtonPress:
	    buttons[button >> 3] |= 1 << (button & 7);
	    break;
	case ButtonRelease:
	    buttons[button >> 3] &= ~(1 << (button & 7));
	default:
	    break;
	}

	dmxUpdateButtonState (pDevice, buttons);
    }

    return nEvents;
}

static int
dmxChangeKeyState (DeviceIntPtr pDevice,
		   int          key,
		   int          how)
{
    int nEvents = 0;

    if (key > 0 && key <= 255)
    {
	char keys[32];

	GETDMXLOCALFROMPDEVICE;

	memcpy (keys, dmxLocal->state, sizeof (keys));

	switch (how) {
	case KeyPress:
	    keys[key >> 3] |= 1 << (key & 7);
	    break;
	case KeyRelease:
	    keys[key >> 3] &= ~(1 << (key & 7));
	default:
	    break;
	}

	dmxUpdateKeyState (pDevice, keys);
    }

    return nEvents;
}

static int
dmxUpdateSpritePosition (DeviceIntPtr pDevice,
			 int          x,
			 int          y)
{
    if (x == pDevice->last.valuators[0] && y == pDevice->last.valuators[1])
	return 0;

    return dmxButtonEvent (pDevice, 0, x, y, MotionNotify);
}

/** Get events from the X queue on the backend servers and put the
 * events into the DMX event queue. */
void dmxBackendCollectEvents(DevicePtr pDev,
                             dmxMotionProcPtr motion,
                             dmxEnqueueProcPtr enqueue,
                             dmxCheckSpecialProcPtr checkspecial,
                             DMXBlockType block)
{
    GETPRIVFROMPDEV;
    GETDMXINPUTFROMPRIV;
    XEvent               X;
    DMXScreenInfo        *dmxScreen;
    DeviceIntPtr         pButtonDev, pKeyDev;
    TestEventData        data = { dmxInput, &X };

    while ((dmxScreen = dmxBackendGetEvent(priv, (void *) &data)))
    {
	switch (X.type) {
	case MotionNotify:
	    pButtonDev = dmxGetButtonDevice (dmxInput, data.deviceId);
	    if (pButtonDev)
		dmxUpdateSpritePosition (pButtonDev, X.xmotion.x, X.xmotion.y);
	    break;
	case ButtonPress:
	case ButtonRelease:
	    pButtonDev = dmxGetButtonDevice (dmxInput, data.deviceId);
	    if (pButtonDev)
	    {
		dmxUpdateSpritePosition (pButtonDev, X.xbutton.x, X.xbutton.y);
		dmxChangeButtonState (pButtonDev,
				      X.xbutton.button,
				      X.type);
	    }
	    break;
	case KeyPress:
	case KeyRelease:
	    pKeyDev = dmxGetKeyboardDevice (dmxInput, data.deviceId);
	    if (pKeyDev)
	    {
		pButtonDev = dmxGetPairedButtonDevice (dmxInput, pKeyDev);
		if (pButtonDev)
		    dmxUpdateSpritePosition (pButtonDev, X.xkey.x, X.xkey.y);

		dmxChangeKeyState (pKeyDev,
				   X.xkey.keycode,
				   X.type);
	    }
	    break;
	case KeymapNotify:
	    pKeyDev = dmxGetKeyboardDevice (dmxInput, data.deviceId);
	    if (pKeyDev)
		dmxUpdateKeyState (pKeyDev, X.xkeymap.key_vector);
	    break;
	default:
	    if (X.type == dmxInput->event[XI_DeviceStateNotify])
	    {
		XDeviceStateNotifyEvent *dsn = (XDeviceStateNotifyEvent *) &X;
		XInputClass		*ic;
		int			i;

		for (i = 0, ic = (XInputClass *) dsn->data;
		     i < dsn->num_classes;
		     ic = (XInputClass *) ((char *) ic + ic->length), i++)
		{
		    if (ic->class & (1 << ButtonClass))
		    {
			pButtonDev = dmxGetButtonDevice (dmxInput,
							 data.deviceId);
			if (pButtonDev)
			{
			    XButtonStatus *bs = (XButtonStatus *) ic;

			    dmxUpdateButtonState (pButtonDev, bs->buttons);
			}
		    }

		    if (ic->class & (1 << KeyClass))
		    {
			pKeyDev = dmxGetKeyboardDevice (dmxInput,
							data.deviceId);
			if (pKeyDev)
			{
			    XKeyStatus *ks = (XKeyStatus *) ic;

			    dmxUpdateButtonState (pKeyDev, ks->keys);
			}
		    }
		}
	    }
	    break;
	}
    }
}

/** Called after input events are processed from the DMX queue.  No
 * event processing actually takes place here, but this is a convenient
 * place to update the pointer. */
void dmxBackendProcessInput(pointer private)
{
    GETPRIVFROMPRIVATE;

    DMXDBG6("dmxBackendProcessInput: myScreen=%d relative=%d"
            " last=%d,%d center=%d,%d\n",
            priv->myScreen, priv->relative,
            priv->lastX, priv->lastY,
            priv->centerX, priv->centerY);

    if (priv->relative
        && !dmxInput->console
        && (priv->lastX != priv->centerX || priv->lastY != priv->centerY)) {
        DMXDBG4("   warping pointer from last=%d,%d to center=%d,%d\n",
                priv->lastX, priv->lastY, priv->centerX, priv->centerY);
	priv->lastX   = priv->centerX;
	priv->lastY   = priv->centerY;
	XLIB_PROLOGUE (&dmxScreens[priv->myScreen]);
        XWarpPointer(priv->display, None, priv->window,
                     0, 0, 0, 0, priv->lastX, priv->lastY);
	XLIB_EPILOGUE (&dmxScreens[priv->myScreen]);
        dmxSync(&dmxScreens[priv->myScreen], TRUE);
    }
}

void dmxBackendGrabButton(DevicePtr pDev,
			  DevicePtr pModDev,
			  WindowPtr pWindow,
			  WindowPtr pConfineTo,
			  int	    button,
			  int	    modifiers,
			  CursorPtr pCursor)
{
    GETPRIVFROMPDEV;
    GETDMXINPUTFROMPRIV;
    ScreenPtr     pScreen = screenInfo.screens[dmxInput->scrnIdx];
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Window        confineTo = None;
    Cursor        cursor = None;

    if (pConfineTo)
	confineTo = (DMX_GET_WINDOW_PRIV (pConfineTo))->window;

    if (pCursor)
	cursor = (DMX_GET_CURSOR_PRIV (pCursor, pScreen))->cursor;

    if (dmxLocal->deviceId >= 0)
    {
	XDevice *modDev = NULL;

	if (pModDev)
	    modDev = ((DMXLocalInputInfoPtr) pModDev->devicePrivate)->device;

	/* this is really useless as XGrabDeviceButton doesn't allow us
	   to specify a confineTo window or cursor */
	XLIB_PROLOGUE (dmxScreen);
	XGrabDeviceButton (dmxScreen->beDisplay,
			   dmxLocal->device,
			   button,
			   modifiers,
			   modDev,
			   (DMX_GET_WINDOW_PRIV (pWindow))->window,
			   TRUE,
			   0,
			   NULL,
			   GrabModeAsync,
			   GrabModeAsync);
	XLIB_EPILOGUE (dmxScreen);
    }
    else
    {
	XLIB_PROLOGUE (dmxScreen);
	XGrabButton (dmxScreen->beDisplay,
		     button,
		     modifiers,
		     (DMX_GET_WINDOW_PRIV (pWindow))->window,
		     TRUE,
		     0,
		     GrabModeAsync,
		     GrabModeAsync,
		     confineTo,
		     cursor);
	XLIB_EPILOGUE (dmxScreen);
    }
}

void dmxBackendUngrabButton(DevicePtr pDev,
			    DevicePtr pModDev,
			    WindowPtr pWindow,
			    int	      button,
			    int	      modifiers)
{
    GETPRIVFROMPDEV;
    GETDMXINPUTFROMPRIV;
    ScreenPtr     pScreen = screenInfo.screens[dmxInput->scrnIdx];
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxLocal->deviceId >= 0)
    {
	XDevice *modDev = NULL;

	if (pModDev)
	    modDev = ((DMXLocalInputInfoPtr) pModDev->devicePrivate)->device;

	XLIB_PROLOGUE (dmxScreen);
	XUngrabDeviceButton (dmxScreen->beDisplay,
			     dmxLocal->device,
			     button,
			     modifiers,
			     modDev,
			     (DMX_GET_WINDOW_PRIV (pWindow))->window);
	XLIB_EPILOGUE (dmxScreen);
    }
    else
    {
	XLIB_PROLOGUE (dmxScreen);
	XUngrabButton (dmxScreen->beDisplay,
		       button,
		       modifiers,
		       (DMX_GET_WINDOW_PRIV (pWindow))->window);
	XLIB_EPILOGUE (dmxScreen);
    }
}

void dmxBackendGrabPointer(DevicePtr pDev,
			   WindowPtr pWindow,
			   WindowPtr pConfineTo,
			   CursorPtr pCursor)
{
    GETPRIVFROMPDEV;
    GETDMXINPUTFROMPRIV;
    ScreenPtr     pScreen = screenInfo.screens[dmxInput->scrnIdx];
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Window        confineTo = None;
    Cursor        cursor = None;

    if (pConfineTo)
	confineTo = (DMX_GET_WINDOW_PRIV (pConfineTo))->window;

    if (pCursor)
	cursor = (DMX_GET_CURSOR_PRIV (pCursor, pScreen))->cursor;

    if (dmxLocal->deviceId >= 0)
    {
	XLIB_PROLOGUE (dmxScreen);
	XExtendedGrabDevice (dmxScreen->beDisplay,
			     dmxLocal->device,
			     (DMX_GET_WINDOW_PRIV (pWindow))->window,
			     GrabModeAsync,
			     TRUE,
			     confineTo,
			     cursor,
			     0,
			     NULL,
			     0,
			     NULL);
	XLIB_EPILOGUE (dmxScreen);
    }
    else
    {
	XLIB_PROLOGUE (dmxScreen);
	XGrabPointer (dmxScreen->beDisplay,
		      (DMX_GET_WINDOW_PRIV (pWindow))->window,
		      TRUE,
		      0,
		      GrabModeAsync,
		      GrabModeAsync,
		      confineTo,
		      cursor,
		      CurrentTime);
	XLIB_EPILOGUE (dmxScreen);
    }
}

void dmxBackendUngrabPointer(DevicePtr pDev,
			     WindowPtr pWindow)
{
    GETPRIVFROMPDEV;
    GETDMXINPUTFROMPRIV;
    ScreenPtr     pScreen = screenInfo.screens[dmxInput->scrnIdx];
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxLocal->deviceId >= 0)
    {
	XLIB_PROLOGUE (dmxScreen);
	XUngrabDevice (dmxScreen->beDisplay, dmxLocal->device, CurrentTime);
	XLIB_EPILOGUE (dmxScreen);
    }
    else
    {
	XLIB_PROLOGUE (dmxScreen);
	XUngrabPointer (dmxScreen->beDisplay, CurrentTime);
	XLIB_EPILOGUE (dmxScreen);
    }
}

static void dmxBackendComputeCenter(myPrivate *priv)
{
    int centerX;
    int centerY;
    
    centerX       = priv->be->rootWidth / 2 + priv->be->rootX;
    centerY       = priv->be->rootHeight / 2 + priv->be->rootY;

    if (centerX > priv->be->rootWidth)  centerX = priv->be->rootWidth  - 1;
    if (centerY > priv->be->rootHeight) centerY = priv->be->rootHeight - 1;
    if (centerX < 1)                    centerX = 1;
    if (centerY < 1)                    centerY = 1;

    priv->centerX = centerX;
    priv->centerY = centerY;
}

static DMXScreenInfo *dmxBackendInitPrivate(DevicePtr pDev)
{
    GETPRIVFROMPDEV;
    DMXInputInfo      *dmxInput = &dmxInputs[dmxLocal->inputIdx];
    DMXScreenInfo     *dmxScreen;
    int               i;

    /* Fill in myPrivate */
    for (i = 0,dmxScreen = &dmxScreens[0]; i<dmxNumScreens; i++,dmxScreen++) {
        if ((dmxInput->scrnIdx == -1 &&
	     dmxPropertySameDisplay (dmxScreen, dmxInput->name)) ||
	    dmxInput->scrnIdx == i) {
            priv->display  = dmxScreen->beDisplay;
            priv->window   = dmxScreen->scrnWin;
            priv->be       = dmxScreen;
            break;
        }
    }

    if (i >= dmxNumScreens)
        dmxLog(dmxFatal,
               "%s is not an existing backend display - cannot initialize\n",
               dmxInput->name);

    return dmxScreen;
}

/** Re-initialized the backend device described by \a pDev (after a
 * reconfig). */
void dmxBackendLateReInit(DevicePtr pDev)
{
    GETPRIVFROMPDEV;
    int               x, y;

    DMXDBG1("dmxBackendLateReInit miPointerCurrentScreen() = %p\n",
            miPointerCurrentScreen());

    dmxBackendSameDisplay(NULL, 0); /* Invalidate cache */
    dmxBackendInitPrivate(pDev);
    dmxBackendComputeCenter(priv);
    dmxGetGlobalPosition(&x, &y);
    dmxInvalidateGlobalPosition(); /* To force event processing */
}

/** Initialized the backend device described by \a pDev. */
void dmxBackendInit(DevicePtr pDev)
{
    GETPRIVFROMPDEV;
    DMXScreenInfo     *dmxScreen;

    dmxBackendSameDisplay(NULL, 0); /* Invalidate cache */

    if (dmxLocal->type == DMX_LOCAL_MOUSE)    priv->mou = pDev;
    if (dmxLocal->type == DMX_LOCAL_KEYBOARD) priv->kbd = pDev;
    if (priv->initialized++) return; /* Only do once for mouse/keyboard pair */

    dmxScreen = dmxBackendInitPrivate(pDev);

    /* Finish initialization using computed values or constants. */
    dmxBackendComputeCenter(priv);
    priv->eventMask          = StructureNotifyMask | SubstructureRedirectMask;
    priv->myScreen           = dmxScreen->index;
    priv->lastX              = priv->centerX;
    priv->lastY              = priv->centerY;
    priv->relative           = 0;
    priv->newscreen          = 0;
}

/** Get information about the backend pointer (for initialization). */
void dmxBackendMouGetInfo(DevicePtr pDev, DMXLocalInitInfoPtr info)
{
    const DMXScreenInfo *dmxScreen = dmxBackendInitPrivate(pDev);

    info->buttonClass      = 1;
    dmxCommonMouGetMap(pDev, info->map, &info->numButtons);
    info->valuatorClass    = 1;
    info->numRelAxes       = 2;
    info->minval[0]        = 0;
    info->minval[1]        = 0;
    info->maxval[0]        = dmxScreen->beWidth;
    info->maxval[1]        = dmxScreen->beHeight;
    info->res[0]           = 1;
    info->minres[0]        = 0;
    info->maxres[0]        = 1;
    info->ptrFeedbackClass = 1;
}

/** Get information about the backend keyboard (for initialization). */
void dmxBackendKbdGetInfo(DevicePtr pDev, DMXLocalInitInfoPtr info)
{
    dmxCommonKbdGetInfo(pDev, info);
    info->keyboard         = 1;
    info->keyClass         = 1;
    dmxCommonKbdGetMap(pDev, &info->keySyms, info->modMap);
    info->freemap          = 1;
    info->focusClass       = 1;
    info->kbdFeedbackClass = 1;
}

/** Process #DMXFunctionType functions.  The only function handled here
 * is to acknowledge a pending server shutdown. */
int dmxBackendFunctions(pointer private, DMXFunctionType function)
{
    switch (function) {
    case DMX_FUNCTION_TERMINATE:
        return 1;
    default:
        return 0;
    }
}

void dmxBackendKbdOff(DevicePtr pDev)
{
    char state[32];

    memset (state, 0, sizeof (state));
    
    dmxUpdateKeyState ((DeviceIntPtr) pDev, state);
    dmxUpdateButtonState ((DeviceIntPtr) pDev, state);
    
    dmxCommonKbdOff (pDev);
}
