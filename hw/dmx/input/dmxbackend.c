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
#include "dmxcursor.h"
#include "dmxwindow.h"
#include "dmxprop.h"
#include "dmxsync.h"
#include "dmxscrinit.h"
#include "dmxxlibio.h"
#include "dmxcb.h"

#include "inputstr.h"
#include "input.h"
#include <X11/keysym.h>
#include <xcb/xinput.h>
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
	case XCB_BUTTON_PRESS:
	    dmxLocal->state[button >> 3] |= 1 << (button & 7);
	    break;
	case XCB_BUTTON_RELEASE:
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
	case XCB_KEY_PRESS:
	    dmxLocal->state[key >> 3] |= 1 << (key & 7);
	    break;
	case XCB_KEY_RELEASE:
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
					   XCB_BUTTON_RELEASE);

	    /* button should be down, but isn't */
	    if (!(dmxLocal->state[i] & (1 << j)) && (buttons[i] & (1 << j)))
		nEvents += dmxButtonEvent (pDevice,
					   (i << 3) + j,
					   pDevice->last.valuators[0],
					   pDevice->last.valuators[1],
					   XCB_BUTTON_PRESS);
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
					XCB_KEY_RELEASE);

	    /* key should be down, but isn't */
	    if (!(dmxLocal->state[i] & (1 << j)) && (keys[i] & (1 << j)))
		nEvents += dmxKeyEvent (pDevice,
					(i << 3) + j,
					XCB_KEY_PRESS);
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
	case XCB_BUTTON_PRESS:
	    buttons[button >> 3] |= 1 << (button & 7);
	    break;
	case XCB_BUTTON_RELEASE:
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
	case XCB_KEY_PRESS:
	    keys[key >> 3] |= 1 << (key & 7);
	    break;
	case XCB_KEY_RELEASE:
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

    return dmxButtonEvent (pDevice, 0, x, y, XCB_MOTION_NOTIFY);
}

Bool
dmxBackendPointerEventCheck (DevicePtr           pDev,
			     xcb_generic_event_t *event)
{
    GETPRIVFROMPDEV;
    GETDMXINPUTFROMPRIV;
    DeviceIntPtr pButtonDev = (DeviceIntPtr) pDev;
    int          reltype, type = event->response_type & 0x7f;
    int          id = dmxLocal->deviceId;

    switch (type) {
    case XCB_MOTION_NOTIFY: {
	xcb_motion_notify_event_t *xmotion =
	    (xcb_motion_notify_event_t *) event;
		    
	dmxUpdateSpritePosition (pButtonDev,
				 xmotion->event_x,
				 xmotion->event_y);
    } break;
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
	xcb_button_press_event_t *xbutton =
	    (xcb_button_press_event_t *) event;

	dmxUpdateSpritePosition (pButtonDev,
				 xbutton->event_x,
				 xbutton->event_y);
	dmxChangeButtonState (pButtonDev,
			      xbutton->detail,
			      type);
    } break;
    default:
	if (id < 0)
	    return FALSE;

	reltype = type - dmxInput->eventBase;

	switch (reltype) {
	case XCB_INPUT_DEVICE_VALUATOR: {
	    xcb_input_device_valuator_event_t *xvaluator =
		(xcb_input_device_valuator_event_t *) event;

	    if (id != (xvaluator->device_id & DEVICE_BITS))
		return FALSE;

	    /* eat valuator events */
	} break;
	case XCB_INPUT_DEVICE_MOTION_NOTIFY: {
	    xcb_input_device_button_release_event_t *xmotion =
		(xcb_input_device_button_release_event_t *) event;

	    dmxUpdateSpritePosition (pButtonDev,
				     xmotion->event_x,
				     xmotion->event_y);
	} break;
	case XCB_INPUT_DEVICE_BUTTON_RELEASE:
	case XCB_INPUT_DEVICE_BUTTON_PRESS: {
	    xcb_input_device_key_press_event_t *xbutton =
		(xcb_input_device_key_press_event_t *) event;

	    if (id != (xbutton->device_id & DEVICE_BITS))
		return FALSE;

	    dmxUpdateSpritePosition (pButtonDev,
				     xbutton->event_x,
				     xbutton->event_y);
	    dmxChangeButtonState (pButtonDev,
				  xbutton->detail, XCB_BUTTON_RELEASE +
				  (reltype -
				   XCB_INPUT_DEVICE_BUTTON_RELEASE));
	} break;
	case XCB_INPUT_DEVICE_STATE_NOTIFY: {
	    xcb_input_device_state_notify_event_t *xstate =
		(xcb_input_device_state_notify_event_t *) event;

	    if (id != (xstate->device_id & DEVICE_BITS))
		return FALSE;

	    if (!(xstate->classes_reported & (1 << ButtonClass)))
		return FALSE;

	    memset (dmxLocal->keysbuttons, 0, 32);

	    if (xstate->response_type & MORE_EVENTS)
	    {
		memcpy (dmxLocal->keysbuttons, xstate->buttons, 4);
	    }
	    else
	    {
		memcpy (dmxLocal->keysbuttons, xstate->buttons, 4);
		dmxUpdateButtonState (pButtonDev,
				      dmxLocal->keysbuttons);
	    }
	} break;
	case XCB_INPUT_DEVICE_BUTTON_STATE_NOTIFY: {
	    xcb_input_device_button_state_notify_event_t *xstate =
		(xcb_input_device_button_state_notify_event_t *) event;

	    if (id != (xstate->device_id & DEVICE_BITS))
		return FALSE;

	    memcpy (&dmxLocal->keysbuttons[4], xstate->buttons, 28);
	    dmxUpdateButtonState (pButtonDev, dmxLocal->keysbuttons);
	} break;
	default:
	    return FALSE;
	}
    }

    return TRUE;
}

/* not in xcb-xinput yet */
#define DMX_XCB_INPUT_EXTENDED_GRAB_DEVICE 45

typedef struct dmx_xcb_input_extended_grab_device_request_t {
    uint8_t         major_opcode;
    uint8_t         minor_opcode;
    uint16_t        length;
    xcb_window_t    grab_window;
    xcb_timestamp_t time;
    uint8_t         deviceid;
    uint8_t         device_mode;
    uint8_t         owner_events;
    uint8_t         pad0;
    xcb_window_t    confine_to;
    xcb_cursor_t    cursor;
    uint16_t        event_count;
    uint16_t        generic_event_count;
} dmx_xcb_input_extended_grab_device_request_t;

Bool
dmxBackendPointerReplyCheck (DevicePtr           pDev,
			     unsigned int        request,
			     xcb_generic_reply_t *reply)
{
    GETDMXLOCALFROMPDEV;

    if (request == dmxLocal->grab.sequence)
    {
	xcb_grab_status_t status = XCB_GRAB_STATUS_FROZEN;

	if (reply->response_type == 1)
	{
	    if (dmxLocal->deviceId >= 0)
	    {
		xcb_input_grab_device_reply_t *xgrab =
		    (xcb_input_grab_device_reply_t *) reply;

		status = xgrab->status;
	    }
	    else
	    {
		xcb_grab_pointer_reply_t *xgrab =
		    (xcb_grab_pointer_reply_t *) reply;

		status = xgrab->status;
	    }
	}

	if (status == XCB_GRAB_STATUS_SUCCESS)
	{
	    /* TODO: track state of grabs */
	}

	dmxLocal->grab.sequence = 0;
	return TRUE;
    }

    return FALSE;
}

Bool
dmxBackendKeyboardEventCheck (DevicePtr           pDev,
			      xcb_generic_event_t *event)
{
    GETPRIVFROMPDEV;
    GETDMXINPUTFROMPRIV;
    DeviceIntPtr pKeyDev = (DeviceIntPtr) pDev;
    DeviceIntPtr pButtonDev;
    int          reltype, type = event->response_type & 0x7f;
    int          id = dmxLocal->deviceId;

    switch (type) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
	xcb_key_press_event_t *xkey = (xcb_key_press_event_t *) event;
	
	pButtonDev = dmxGetPairedButtonDevice (dmxInput, pKeyDev);
	if (pButtonDev)
	    dmxUpdateSpritePosition (pButtonDev, xkey->event_x, xkey->event_y);

	dmxChangeKeyState (pKeyDev,
			   xkey->detail,
			   type);
    } break;
    case XCB_KEYMAP_NOTIFY: {
	xcb_keymap_notify_event_t *xkeymap =
	    (xcb_keymap_notify_event_t *) event;
	char state[32];

	state[0] = 0;
	memcpy (&state[1], xkeymap->keys, 31);
    
	dmxUpdateKeyState (pKeyDev, state);
    } break;
    default:
	if (id < 0)
	    return FALSE;

	reltype = type - dmxInput->eventBase;

	switch (reltype) {
	case XCB_INPUT_DEVICE_VALUATOR: {
	    xcb_input_device_valuator_event_t *xvaluator =
		(xcb_input_device_valuator_event_t *) event;

	    if (id != (xvaluator->device_id & DEVICE_BITS))
		return FALSE;

	    /* eat valuator events */
	} break;
	case XCB_INPUT_DEVICE_KEY_PRESS:
	case XCB_INPUT_DEVICE_KEY_RELEASE: {
	    xcb_input_device_key_press_event_t *xkey =
		(xcb_input_device_key_press_event_t *) event;

	    if (id != (xkey->device_id & DEVICE_BITS))
		return FALSE;

	    pButtonDev = dmxGetPairedButtonDevice (dmxInput, pKeyDev);
	    if (pButtonDev)
		dmxUpdateSpritePosition (pButtonDev,
					 xkey->event_x,
					 xkey->event_y);

	    dmxChangeKeyState (pKeyDev,
			       xkey->detail, XCB_KEY_PRESS +
			       (reltype - XCB_INPUT_DEVICE_KEY_PRESS));
	} break;
	case XCB_INPUT_DEVICE_STATE_NOTIFY: {
	    xcb_input_device_state_notify_event_t *xstate =
		(xcb_input_device_state_notify_event_t *) event;

	    if (id != (xstate->device_id & DEVICE_BITS))
		return FALSE;

	    if (!(xstate->classes_reported & (1 << KeyClass)))
		return FALSE;

	    memset (dmxLocal->keysbuttons, 0, 32);

	    if (xstate->response_type & MORE_EVENTS)
	    {
		memcpy (dmxLocal->keysbuttons, xstate->keys, 4);
	    }
	    else
	    {
		memcpy (dmxLocal->keysbuttons, xstate->keys, 4);
		dmxUpdateKeyState (pKeyDev, dmxLocal->keysbuttons);
	    }
	} break;
	case XCB_INPUT_DEVICE_KEY_STATE_NOTIFY: {
	    xcb_input_device_key_state_notify_event_t *xstate =
		(xcb_input_device_key_state_notify_event_t *) event;

	    if (id != (xstate->device_id & DEVICE_BITS))
		return FALSE;

	    memcpy (&dmxLocal->keysbuttons[4], xstate->keys, 28);
	    dmxUpdateKeyState (pKeyDev, dmxLocal->keysbuttons);
	} break;
	default:
	    return FALSE;
	}
    }

    return TRUE;
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
    Window        window = (DMX_GET_WINDOW_PRIV (pWindow))->window;
    Window        confineTo = None;
    Cursor        cursor = None;

    if (pConfineTo)
	confineTo = (DMX_GET_WINDOW_PRIV (pConfineTo))->window;

    if (pCursor)
	cursor = (DMX_GET_CURSOR_PRIV (pCursor, pScreen))->cursor;

    if (dmxLocal->deviceId >= 0)
    {
	int id = dmxLocal->deviceId;
	int modId = 0;

	if (pModDev)
	    modId = ((DMXLocalInputInfoPtr) pModDev->devicePrivate)->deviceId;

	/* this is really useless as XGrabDeviceButton doesn't allow us
	   to specify a confineTo window or cursor */
	xcb_input_grab_device_button (dmxScreen->connection,
				      window,
				      id,
				      modId,
				      0,
				      modifiers,
				      XCB_GRAB_MODE_ASYNC,
				      XCB_GRAB_MODE_ASYNC,
				      button,
				      TRUE,
				      NULL);
    }
    else
    {
	xcb_grab_button (dmxScreen->connection,
			 TRUE,
			 window,
			 0,
			 XCB_GRAB_MODE_ASYNC,
			 XCB_GRAB_MODE_ASYNC,
			 confineTo,
			 cursor,
			 button,
			 modifiers);
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
    Window        window = (DMX_GET_WINDOW_PRIV (pWindow))->window;

    if (dmxLocal->deviceId >= 0)
    {
	int id = dmxLocal->deviceId;
	int modId = 0;

	if (pModDev)
	    modId = ((DMXLocalInputInfoPtr) pModDev->devicePrivate)->deviceId;

	xcb_input_ungrab_device_button (dmxScreen->connection,
					window,
					modifiers,
					modId,
					button,
					id);
    }
    else
    {
	xcb_ungrab_button (dmxScreen->connection,
			   button,
			   window,
			   modifiers);
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
    Window        window = (DMX_GET_WINDOW_PRIV (pWindow))->window;
    Window        confineTo = None;
    Cursor        cursor = None;

    if (pConfineTo)
	confineTo = (DMX_GET_WINDOW_PRIV (pConfineTo))->window;

    if (pCursor)
	cursor = (DMX_GET_CURSOR_PRIV (pCursor, pScreen))->cursor;

    if (dmxLocal->deviceId >= 0)
    {
	dmx_xcb_input_extended_grab_device_request_t grab = {
	    .grab_window  = window,
	    .deviceid     = dmxLocal->deviceId,
	    .device_mode  = XCB_GRAB_MODE_ASYNC,
	    .owner_events = TRUE,
	    .confine_to   = confineTo,
	    .cursor       = cursor
	};
	xcb_protocol_request_t request = {
	    1,
	    &xcb_input_id,
	    DMX_XCB_INPUT_EXTENDED_GRAB_DEVICE,
	    FALSE
	};
	struct iovec vector = { &grab, sizeof (grab) };

	dmxLocal->grab.sequence =
	    xcb_send_request (dmxScreen->connection,
			      0,
			      &vector,
			      &request);
    }
    else
    {
	dmxLocal->grab.sequence =
	    xcb_grab_pointer (dmxScreen->connection,
			      TRUE,
			      window,
			      0,
			      XCB_GRAB_MODE_ASYNC,
			      XCB_GRAB_MODE_ASYNC,
			      confineTo,
			      cursor,
			      0).sequence;
    }

    dmxAddSequence (&dmxScreen->request, dmxLocal->grab.sequence);
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
	xcb_input_ungrab_device (dmxScreen->connection,
				 0,
				 dmxLocal->deviceId);
    }
    else
    {
	xcb_ungrab_pointer (dmxScreen->connection, 0);
    }
}

static DMXScreenInfo *dmxBackendInitPrivate(DevicePtr pDev)
{
    GETPRIVFROMPDEV;
    DMXInputInfo      *dmxInput = &dmxInputs[dmxLocal->inputIdx];
    DMXScreenInfo     *dmxScreen;
    int               i;

    /* Fill in myPrivate */
    for (i = 0,dmxScreen = &dmxScreens[0]; i<dmxNumScreens; i++,dmxScreen++) {
        if (dmxInput->scrnIdx == i) {
            priv->display  = dmxScreen->beDisplay;
            priv->window   = dmxScreen->rootWin;
            priv->be       = dmxScreen;
            break;
        }
    }

    return dmxScreen;
}

/** Re-initialized the backend device described by \a pDev (after a
 * reconfig). */
void dmxBackendLateReInit(DevicePtr pDev)
{
    DMXDBG1("dmxBackendLateReInit miPointerCurrentScreen() = %p\n",
            miPointerCurrentScreen());

    dmxBackendInitPrivate(pDev);
}

/** Initialized the backend device described by \a pDev. */
void dmxBackendInit(DevicePtr pDev)
{
    GETPRIVFROMPDEV;
    DMXScreenInfo     *dmxScreen;

    if (dmxLocal->type == DMX_LOCAL_MOUSE)    priv->mou = pDev;
    if (dmxLocal->type == DMX_LOCAL_KEYBOARD) priv->kbd = pDev;
    if (priv->initialized++) return; /* Only do once for mouse/keyboard pair */

    dmxScreen = dmxBackendInitPrivate(pDev);

    /* Finish initialization using computed values or constants. */
    priv->eventMask          = ExposureMask | SubstructureRedirectMask;
    priv->myScreen           = dmxScreen->index;
    priv->lastX              = 0;
    priv->lastY              = 0;
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
