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

#include "dmx.h"
#include "dmxlog.h"
#include "dmxinput.h"
#include "dmxgrab.h"
#include "dmxwindow.h"
#include "dmxcursor.h"
#include "dmxscrinit.h"
#include "dmxxlibio.h"

#include "inputstr.h"
#include "input.h"
#include "mi.h"
#include "exevents.h"
#include "XIstubs.h"
#include "xace.h"

#include <X11/keysym.h>
#include <xcb/xinput.h>

#define DMX_KEYBOARD_EVENT_MASK				\
    (KeyPressMask | KeyReleaseMask | KeymapStateMask)

#define DMX_POINTER_EVENT_MASK					\
    (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

static EventListPtr dmxEvents = NULL;

static void
dmxUpdateKeycodeMap (DeviceIntPtr pDevice)
{
    dmxDevicePrivPtr  pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo     *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;
    const xcb_setup_t *setup = xcb_get_setup (dmxScreen->connection);
    KeySymsPtr        dst = &pDevice->key->curKeySyms;
    KeySymsPtr        src = &pDevPriv->keySyms;
    int               width = src->mapWidth;
    int               i;

    if (!pDevice->isMaster && pDevice->u.master && pDevice->u.master->key)
	dst = &pDevice->u.master->key->curKeySyms;

    if (dst->mapWidth < width)
	width = dst->mapWidth;

    memset (pDevPriv->keycode, 0,
	    sizeof (KeyCode) * (setup->max_keycode - setup->min_keycode));

    for (i = src->minKeyCode - setup->min_keycode;
	 i < (src->maxKeyCode - src->minKeyCode);
	 i++)
    {
	int j;

	for (j = 0; j < (dst->maxKeyCode - dst->minKeyCode); j++)
	{
	    int k;

	    for (k = 0; k < i; k++)
		if (pDevPriv->keycode[k] == (dst->minKeyCode + j))
		    break;

	    /* make sure the keycode is not already in use */
	    if (k < i)
		continue;

	    for (k = 0; k < width; k++)
		if (dst->map[j * dst->mapWidth + k] != NoSymbol &&
		    src->map[i * src->mapWidth + k] != NoSymbol)
		    if (dst->map[j * dst->mapWidth + k] !=
			src->map[i * src->mapWidth + k])
			break;

	    if (k == width)
		break;
	}

	if (j < (dst->maxKeyCode - dst->minKeyCode))
	    pDevPriv->keycode[i] = dst->minKeyCode + j;
    }
}

static void
dmxUpdateKeyboardMapping (DeviceIntPtr pDevice,
			  int          first,
			  int          count)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;
    KeySymsRec       keysyms;
    xcb_keysym_t     *syms;
    int              n;

    if (pDevPriv->deviceId >= 0)
    {
	xcb_input_get_device_key_mapping_reply_t *reply;

	reply = xcb_input_get_device_key_mapping_reply
	    (dmxScreen->connection,
	     xcb_input_get_device_key_mapping (dmxScreen->connection,
					       pDevPriv->deviceId,
					       first,
					       count),
	     NULL);

	if (reply)
	{
	    syms = xcb_input_get_device_key_mapping_keysyms (reply);
	    n    = xcb_input_get_device_key_mapping_keysyms_length (reply);

	    keysyms.minKeyCode = first;
	    keysyms.maxKeyCode = first + n - 1;
	    keysyms.mapWidth   = reply->keysyms_per_keycode;
	    keysyms.map        = (KeySym *) syms;
    
	    SetKeySymsMap (&pDevPriv->keySyms, &keysyms);

	    free (reply);
	}
    }
    else
    {
	xcb_get_keyboard_mapping_reply_t *reply;

	reply = xcb_get_keyboard_mapping_reply
	    (dmxScreen->connection,
	     xcb_get_keyboard_mapping (dmxScreen->connection,
				       first,
				       count),
	     NULL);

	if (reply)
	{
	    syms = xcb_get_keyboard_mapping_keysyms (reply);
	    n    = xcb_get_keyboard_mapping_keysyms_length (reply);

	    keysyms.minKeyCode = first;
	    keysyms.maxKeyCode = first + n - 1;
	    keysyms.mapWidth   = reply->keysyms_per_keycode;
	    keysyms.map        = (KeySym *) syms;
    
	    SetKeySymsMap (&pDevPriv->keySyms, &keysyms);

	    free (reply);
	}
    }

    dmxUpdateKeycodeMap (pDevice);
}

static DeviceIntPtr
dmxGetButtonDevice (DMXInputInfo *dmxInput,
		    XID          deviceId)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (dmxInput->devs[i]);

	if (!dmxInput->devs[i]->button)
	    continue;
	
	if (deviceId >= 0)
	{
	    if (pDevPriv->deviceId != deviceId)
		continue;
	}

	return dmxInput->devs[i];
    }

    return NULL;
}

static DeviceIntPtr
dmxGetPairedButtonDevice (DeviceIntPtr pDev)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDev);

    return dmxGetButtonDevice (pDevPriv->dmxInput, pDevPriv->masterId);
}

static int
dmxButtonEvent (DeviceIntPtr pDevice,
		int          button,
		int          x,
		int          y,
		int          type)
{
    int v[2] = { x, y };
    int nEvents, i;

    GetEventList (&dmxEvents);
    nEvents = GetPointerEvents (dmxEvents,
				pDevice,
				type,
				button,
				POINTER_ABSOLUTE,
				0, 2,
				v);

    for (i = 0; i < nEvents; i++)
	mieqEnqueue (pDevice, dmxEvents[i].event);

    if (button > 0 && button <= 255)
    {
	dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);

	switch (type) {
	case XCB_BUTTON_PRESS:
	    pDevPriv->state[button >> 3] |= 1 << (button & 7);
	    break;
	case XCB_BUTTON_RELEASE:
	    pDevPriv->state[button >> 3] &= ~(1 << (button & 7));
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
    dmxDevicePrivPtr  pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo     *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;
    const xcb_setup_t *setup = xcb_get_setup (dmxScreen->connection);
    int               nEvents = 0;

    if (key >= setup->min_keycode && key <= setup->max_keycode)
    {
	int keycode = pDevPriv->keycode[key - setup->min_keycode];

	if (keycode)
	{
	    int i;

	    GetEventList (&dmxEvents);
	    nEvents = GetKeyboardEvents (dmxEvents, pDevice, type, keycode);
	    for (i = 0; i < nEvents; i++)
		mieqEnqueue (pDevice, dmxEvents[i].event);
	}
    }

    if (key > 0 && key <= 255)
    {
	switch (type) {
	case XCB_KEY_PRESS:
	    pDevPriv->state[key >> 3] |= 1 << (key & 7);
	    break;
	case XCB_KEY_RELEASE:
	    pDevPriv->state[key >> 3] &= ~(1 << (key & 7));
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
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    int              i, j, nEvents = 0;

    for (i = 0; i < 32; i++)
    {
	if (!(pDevPriv->state[i] ^ buttons[i]))
	    continue;
	
	for (j = 0; j < 8; j++)
	{
	    /* button is down, but shouldn't be */
	    if ((pDevPriv->state[i] & (1 << j)) && !(buttons[i] & (1 << j)))
		nEvents += dmxButtonEvent (pDevice,
					   (i << 3) + j,
					   pDevice->last.valuators[0],
					   pDevice->last.valuators[1],
					   XCB_BUTTON_RELEASE);

	    /* button should be down, but isn't */
	    if (!(pDevPriv->state[i] & (1 << j)) && (buttons[i] & (1 << j)))
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
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    int              i, j, nEvents = 0;

    for (i = 0; i < 32; i++)
    {
	if (!(pDevPriv->state[i] ^ keys[i]))
	    continue;

	for (j = 0; j < 8; j++)
	{
	    /* key is down, but shouldn't be */
	    if ((pDevPriv->state[i] & (1 << j)) && !(keys[i] & (1 << j)))
		nEvents += dmxKeyEvent (pDevice,
					(i << 3) + j,
					XCB_KEY_RELEASE);

	    /* key should be down, but isn't */
	    if (!(pDevPriv->state[i] & (1 << j)) && (keys[i] & (1 << j)))
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
	dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
	char             buttons[32];

	memcpy (buttons, pDevPriv->state, sizeof (buttons));

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
	dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
	char             keys[32];

	memcpy (keys, pDevPriv->state, sizeof (keys));

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
dmxFakeMotion (DMXInputInfo *dmxInput,
	       int          x,
	       int          y)
{
    DMXScreenInfo *dmxScreen = (DMXScreenInfo *) dmxInput;
    WindowPtr     pWin = WindowTable[dmxScreen->index];
    GrabRec       newGrab;
    int           i;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
	pWin = WindowTable[0];
#endif

    memset (&newGrab, 0, sizeof (GrabRec));	    

    newGrab.window       = pWin;
    newGrab.resource     = 0;
    newGrab.ownerEvents  = xFalse;
    newGrab.cursor       = NULL;
    newGrab.confineTo    = NullWindow;
    newGrab.eventMask    = NoEventMask;
    newGrab.genericMasks = NULL;
    newGrab.next         = NULL;
    newGrab.keyboardMode = GrabModeAsync;
    newGrab.pointerMode  = GrabModeAsync;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr pDevice = dmxInput->devs[i];

	if (!pDevice->isMaster && pDevice->u.master)
	    pDevice = pDevice->u.master;

	if (!pDevice->button)
	    continue;

	newGrab.device = pDevice;

	if (!dmxActivateFakePointerGrab (pDevice, &newGrab))
	    return FALSE;
    }

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr pDevice = dmxInput->devs[i];

	if (!pDevice->button)
	    continue;

	dmxUpdateSpritePosition (pDevice, x, y);
    }

    return TRUE;
}

void
dmxEndFakeMotion (DMXInputInfo *dmxInput)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr pDevice = dmxInput->devs[i];

	if (!pDevice->isMaster && pDevice->u.master)
	    pDevice = pDevice->u.master;

	if (!pDevice->button)
	    continue;

	dmxDeactivateFakePointerGrab (pDevice);
    }
}

static Bool
dmxDeviceKeyboardReplyCheck (DeviceIntPtr        pDevice,
			     unsigned int        request,
			     xcb_generic_reply_t *reply)
{
    return FALSE;
}

static void
dmxInputGrabPointerReply (ScreenPtr           pScreen,
			  unsigned int        sequence,
			  xcb_generic_reply_t *reply,
			  xcb_generic_error_t *error,
			  void                *data)
{
    DMXInputInfo *dmxInput = &dmxScreens[pScreen->myNum].input;
    int          i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr     pDevice = dmxInput->devs[i];
	dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);

	if ((*pDevPriv->ReplyCheck) (pDevice, sequence, reply))
	    break;
    }
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

static void
dmxDeviceGrabPointer (DeviceIntPtr pDevice,
		      WindowPtr    pWindow,
		      WindowPtr    pConfineTo,
		      CursorPtr    pCursor)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;
    ScreenPtr        pScreen = screenInfo.screens[dmxScreen->index];
    Window           window = (DMX_GET_WINDOW_PRIV (pWindow))->window;
    Window           confineTo = None;
    Cursor           cursor = None;

    if (pConfineTo)
	confineTo = (DMX_GET_WINDOW_PRIV (pConfineTo))->window;

    if (pCursor)
	cursor = (DMX_GET_CURSOR_PRIV (pCursor, pScreen))->cursor;

    if (pDevPriv->deviceId >= 0)
    {
	dmx_xcb_input_extended_grab_device_request_t grab = {
	    .grab_window  = window,
	    .deviceid     = pDevPriv->deviceId,
	    .device_mode  = XCB_GRAB_MODE_ASYNC,
	    .owner_events = TRUE,
	    .confine_to   = confineTo,
	    .cursor       = cursor
	};
	xcb_protocol_request_t request = {
	    2,
	    &xcb_input_id,
	    DMX_XCB_INPUT_EXTENDED_GRAB_DEVICE,
	    FALSE
	};
	XEventClass  cls[3];
	int          type;
	struct iovec vector[] =  {
	    { &grab, sizeof (grab) },
	    { cls,   sizeof (cls)  }
	};

	DeviceMotionNotify (pDevPriv->device, type, cls[0]);
	DeviceButtonPress (pDevPriv->device, type, cls[1]);
	DeviceButtonRelease (pDevPriv->device, type, cls[2]);

	pDevPriv->grab.sequence =
	    xcb_send_request (dmxScreen->connection,
			      0,
			      vector,
			      &request);
    }
    else
    {
	pDevPriv->grab.sequence =
	    xcb_grab_pointer (dmxScreen->connection,
			      TRUE,
			      window,
			      DMX_POINTER_EVENT_MASK,
			      XCB_GRAB_MODE_ASYNC,
			      XCB_GRAB_MODE_ASYNC,
			      confineTo,
			      cursor,
			      0).sequence;
    }

    dmxAddRequest (&dmxScreen->request,
		   dmxInputGrabPointerReply,
		   pDevPriv->grab.sequence,
		   0);
}

static void
dmxDeviceUngrabPointer (DeviceIntPtr pDevice)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;

    if (pDevPriv->deviceId >= 0)
    {
	xcb_input_ungrab_device (dmxScreen->connection,
				 0,
				 pDevPriv->deviceId);
    }
    else
    {
	xcb_ungrab_pointer (dmxScreen->connection, 0);
    }
}

static Bool
dmxDevicePointerReplyCheck (DeviceIntPtr        pDevice,
			    unsigned int        request,
			    xcb_generic_reply_t *reply)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);

    if (request == pDevPriv->grab.sequence)
    {
	xcb_grab_status_t status = XCB_GRAB_STATUS_FROZEN;

	if (reply->response_type == 1)
	{
	    if (pDevPriv->deviceId >= 0)
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

	pDevPriv->grab.sequence = 0;
	return TRUE;
    }

    return FALSE;
}

static void
dmxUpdateSpriteFromEvent (DeviceIntPtr pDevice,
			  xcb_window_t event,
			  int          x,
			  int          y)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;

    if (event != dmxScreen->rootWin)
    {
	DeviceIntPtr pMaster = pDevice;
	WindowPtr    pWin;

	if (!pDevice->isMaster && pDevice->u.master)
	    pMaster = pDevice->u.master;

	if (!pMaster->deviceGrab.grab)
	{
	    dmxDeviceUngrabPointer (pDevice);
	    dmxLog (dmxWarning, "non-root window event without active grab\n");
	    return;
	}

	pWin = WindowTable[dmxScreen->index];
	for (;;)
	{
	    dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pWin);

	    if (pWinPriv->window == event)
		break;

	    if (pWin->firstChild)
	    {
		pWin = pWin->firstChild;
		continue;
	    }

	    while (!pWin->nextSib && (pWin != WindowTable[dmxScreen->index]))
		pWin = pWin->parent;

	    if (pWin == WindowTable[dmxScreen->index])
		break;

	    pWin = pWin->nextSib;
	}

	if (!pWin)
	    return;

	x += pWin->drawable.x;
	y += pWin->drawable.y;
    }

    dmxEndFakeMotion (&dmxScreen->input);
    dmxUpdateSpritePosition (pDevice, x, y);
}

static Bool
dmxDevicePointerEventCheck (DeviceIntPtr        pDevice,
			    xcb_generic_event_t *event)
{
    DeviceIntPtr     pButtonDev = pDevice;
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pButtonDev);
    DMXInputInfo     *dmxInput = pDevPriv->dmxInput;
    int              reltype, type = event->response_type & 0x7f;
    int              id = pDevPriv->deviceId;

    switch (type) {
    case XCB_MOTION_NOTIFY: {
	xcb_motion_notify_event_t *xmotion =
	    (xcb_motion_notify_event_t *) event;

	dmxUpdateSpriteFromEvent (pButtonDev,
				  xmotion->event,
				  xmotion->event_x,
				  xmotion->event_y);
    } break;
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
	xcb_button_press_event_t *xbutton =
	    (xcb_button_press_event_t *) event;

	dmxUpdateSpriteFromEvent (pButtonDev,
				  xbutton->event,
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
	    xcb_input_device_motion_notify_event_t *xmotion =
		(xcb_input_device_motion_notify_event_t *) event;

	    if (id != (xmotion->device_id & DEVICE_BITS))
		return FALSE;

	    dmxUpdateSpriteFromEvent (pButtonDev,
				      xmotion->event,
				      xmotion->event_x,
				      xmotion->event_y);
	} break;
	case XCB_INPUT_DEVICE_BUTTON_PRESS:
	case XCB_INPUT_DEVICE_BUTTON_RELEASE: {
	    xcb_input_device_button_press_event_t *xbutton =
		(xcb_input_device_button_press_event_t *) event;

	    if (id != (xbutton->device_id & DEVICE_BITS))
		return FALSE;

	    dmxUpdateSpriteFromEvent (pButtonDev,
				      xbutton->event,
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

	    memset (pDevPriv->keysbuttons, 0, 32);

	    if (xstate->response_type & MORE_EVENTS)
	    {
		memcpy (pDevPriv->keysbuttons, xstate->buttons, 4);
	    }
	    else
	    {
		memcpy (pDevPriv->keysbuttons, xstate->buttons, 4);
		dmxUpdateButtonState (pButtonDev,
				      pDevPriv->keysbuttons);
	    }
	} break;
	case XCB_INPUT_DEVICE_BUTTON_STATE_NOTIFY: {
	    xcb_input_device_button_state_notify_event_t *xstate =
		(xcb_input_device_button_state_notify_event_t *) event;

	    if (id != (xstate->device_id & DEVICE_BITS))
		return FALSE;

	    memcpy (&pDevPriv->keysbuttons[4], xstate->buttons, 28);
	    dmxUpdateButtonState (pButtonDev, pDevPriv->keysbuttons);
	} break;
	default:
	    return FALSE;
	}
    }

    return TRUE;
}

static Bool
dmxDeviceKeyboardEventCheck (DeviceIntPtr        pDevice,
			     xcb_generic_event_t *event)
{
    DeviceIntPtr     pKeyDev = pDevice;
    DeviceIntPtr     pButtonDev;
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pKeyDev);
    DMXInputInfo     *dmxInput = pDevPriv->dmxInput;
    int              reltype, type = event->response_type & 0x7f;
    int              id = pDevPriv->deviceId;

    switch (type) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
	xcb_key_press_event_t *xkey = (xcb_key_press_event_t *) event;
	
	pButtonDev = dmxGetPairedButtonDevice (pKeyDev);
	if (pButtonDev)
	    dmxUpdateSpriteFromEvent (pButtonDev,
				      xkey->event,
				      xkey->event_x,
				      xkey->event_y);

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
    case XCB_MAPPING_NOTIFY: {
	xcb_mapping_notify_event_t *xmapping =
	    (xcb_mapping_notify_event_t *) event;

	if (xmapping->request == XCB_MAPPING_KEYBOARD)
	    dmxUpdateKeyboardMapping (pKeyDev,
				      xmapping->first_keycode,
				      xmapping->count);
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

	    pButtonDev = dmxGetPairedButtonDevice (pKeyDev);
	    if (pButtonDev)
		dmxUpdateSpriteFromEvent (pButtonDev,
					  xkey->event,
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

	    memset (pDevPriv->keysbuttons, 0, 32);

	    if (xstate->response_type & MORE_EVENTS)
	    {
		memcpy (pDevPriv->keysbuttons, xstate->keys, 4);
	    }
	    else
	    {
		memcpy (pDevPriv->keysbuttons, xstate->keys, 4);
		dmxUpdateKeyState (pKeyDev, pDevPriv->keysbuttons);
	    }
	} break;
	case XCB_INPUT_DEVICE_KEY_STATE_NOTIFY: {
	    xcb_input_device_key_state_notify_event_t *xstate =
		(xcb_input_device_key_state_notify_event_t *) event;

	    if (id != (xstate->device_id & DEVICE_BITS))
		return FALSE;

	    memcpy (&pDevPriv->keysbuttons[4], xstate->keys, 28);
	    dmxUpdateKeyState (pKeyDev, pDevPriv->keysbuttons);
	} break;
	case XCB_INPUT_DEVICE_MAPPING_NOTIFY: {
	    xcb_input_device_mapping_notify_event_t *xmapping =
		(xcb_input_device_mapping_notify_event_t *) event;

	    if (id != (xmapping->device_id & DEVICE_BITS))
		return FALSE;

	    if (xmapping->request == XCB_MAPPING_KEYBOARD)
		dmxUpdateKeyboardMapping (pKeyDev,
					  xmapping->first_keycode,
					  xmapping->count);
	} break;
	default:
	    return FALSE;
	}
    }

    return TRUE;
}

Bool
dmxInputEventCheck (DMXInputInfo        *dmxInput,
		    xcb_generic_event_t *event)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr     pDevice = dmxInput->devs[i];
	dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);

        if ((*pDevPriv->EventCheck) (pDevice, event))
	    return TRUE;
    }

    return FALSE;
}

static void
dmxDeviceGrabButton (DeviceIntPtr pDevice,
		     DeviceIntPtr pModDevice,
		     WindowPtr    pWindow,
		     WindowPtr    pConfineTo,
		     int	  button,
		     int	  modifiers,
		     CursorPtr    pCursor)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;
    ScreenPtr        pScreen = screenInfo.screens[dmxScreen->index];
    Window           window = (DMX_GET_WINDOW_PRIV (pWindow))->window;
    Window           confineTo = None;
    Cursor           cursor = None;

    if (pConfineTo)
	confineTo = (DMX_GET_WINDOW_PRIV (pConfineTo))->window;

    if (pCursor)
	cursor = (DMX_GET_CURSOR_PRIV (pCursor, pScreen))->cursor;

    if (pDevPriv->deviceId >= 0)
    {
	int id = pDevPriv->deviceId;
	int modId = 0;

	if (pModDevice)
	    modId = DMX_GET_DEVICE_PRIV (pModDevice)->deviceId;

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

static void
dmxDeviceUngrabButton (DeviceIntPtr pDevice,
		       DeviceIntPtr pModDevice,
		       WindowPtr    pWindow,
		       int	    button,
		       int	    modifiers)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;
    Window           window = (DMX_GET_WINDOW_PRIV (pWindow))->window;

    if (pDevPriv->deviceId >= 0)
    {
	int id = pDevPriv->deviceId;
	int modId = 0;

	if (pModDevice)
	    modId = DMX_GET_DEVICE_PRIV (pModDevice)->deviceId;

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

void
dmxInputGrabButton (DMXInputInfo *dmxInput,
		    DeviceIntPtr pDevice,
		    DeviceIntPtr pModDevice,
		    WindowPtr    pWindow,
		    WindowPtr    pConfineTo,
		    int	         button,
		    int	         modifiers,
		    CursorPtr    pCursor)
{
    int i, j;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr pExtDevice = dmxInput->devs[i];

	if (pExtDevice->u.master != pDevice)
	    continue;

	if (!pExtDevice->button)
	    continue;

	if (modifiers & AnyModifier)
	{
	    dmxDeviceGrabButton (pExtDevice,
				 NULL,
				 pWindow,
				 pConfineTo,
				 button,
				 modifiers,
				 pCursor);
	}
	else
	{
	    for (j = 0; j < dmxInput->numDevs; j++)
	    {
		DeviceIntPtr pExtModDevice = dmxInput->devs[j];

		if (!pExtModDevice->key)
		    continue;

		if (pExtModDevice->u.master == pModDevice)
		    dmxDeviceGrabButton (pExtDevice,
					 pExtModDevice,
					 pWindow,
					 pConfineTo,
					 button,
					 modifiers,
					 pCursor);
	    }
	}
    }
}

void
dmxInputUngrabButton (DMXInputInfo *dmxInput,
		      DeviceIntPtr pDevice,
		      DeviceIntPtr pModDevice,
		      WindowPtr    pWindow,
		      int	   button,
		      int	   modifiers)
{
    int i, j;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr pExtDevice = dmxInput->devs[i];

	if (pExtDevice->u.master != pDevice)
	    continue;

	if (!pExtDevice->button)
	    continue;

	if (modifiers == AnyModifier)
	{
	    dmxDeviceUngrabButton (pExtDevice,
				   NULL,
				   pWindow,
				   button,
				   modifiers);
	}
	else
	{
	    for (j = 0; j < dmxInput->numDevs; j++)
	    {
		DeviceIntPtr pExtModDevice = dmxInput->devs[j];

		if (!pExtModDevice->key)
		    continue;

		if (pExtModDevice->u.master == pModDevice)
		    dmxDeviceUngrabButton (pExtDevice,
					   pExtModDevice,
					   pWindow,
					   button,
					   modifiers);
	    }
	}
    }
}

void
dmxInputGrabPointer (DMXInputInfo *dmxInput,
		     DeviceIntPtr pDevice,
		     WindowPtr    pWindow,
		     WindowPtr    pConfineTo,
		     CursorPtr    pCursor)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr pExtDevice = dmxInput->devs[i];

	if (pExtDevice->u.master != pDevice)
	    continue;

	if (!pExtDevice->button)
	    continue;

	dmxDeviceGrabPointer (pExtDevice,
			      pWindow,
			      pConfineTo,
			      pCursor);
    }
}

void
dmxInputUngrabPointer (DMXInputInfo *dmxInput,
		       DeviceIntPtr pDevice,
		       WindowPtr    pWindow)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DeviceIntPtr pExtDevice = dmxInput->devs[i];

	if (pExtDevice->u.master != pDevice)
	    continue;

	if (!pExtDevice->button)
	    continue;

	dmxDeviceUngrabPointer (pExtDevice);
    }
}

static void
dmxKeyboardBell (int          volume,
		 DeviceIntPtr pDevice,
		 pointer      arg,
		 int          something)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;

    if (dmxScreen)
	xcb_bell (dmxScreen->connection, volume);
}

static int
dmxKeyboardOn (DeviceIntPtr pDevice)
{
    dmxDevicePrivPtr  pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo     *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;
    const xcb_setup_t *setup = xcb_get_setup (dmxScreen->connection);
    int               nKeycode = setup->max_keycode - setup->min_keycode;

    pDevPriv->keycode = xalloc (nKeycode * sizeof (KeyCode));
    if (!pDevPriv->keycode)
	return -1;

    pDevPriv->keySyms.map        = (KeySym *) NULL;
    pDevPriv->keySyms.mapWidth   = 0;
    pDevPriv->keySyms.minKeyCode = setup->min_keycode;
    pDevPriv->keySyms.maxKeyCode = setup->max_keycode;

    dmxUpdateKeyboardMapping (pDevice, setup->min_keycode, nKeycode);

    if (pDevPriv->deviceId >= 0)
    {
	XEventClass cls[3];
	int         type;

	pDevPriv->device = XOpenDevice (dmxScreen->beDisplay,
					pDevPriv->deviceId);
	if (!pDevPriv->device)
	{
	    dmxLog (dmxWarning, "Cannot open %s device (id=%d) on %s\n",
		    pDevice->name, pDevPriv->deviceId, dmxScreen->name);
	    xfree (pDevPriv->keycode);
	    return -1;
	}

	DeviceKeyPress (pDevPriv->device, type, cls[0]);
	DeviceKeyRelease (pDevPriv->device, type, cls[1]);
	DeviceStateNotify (pDevPriv->device, type, cls[2]);

	XLIB_PROLOGUE (dmxScreen);
	XSelectExtensionEvent (dmxScreen->beDisplay, dmxScreen->rootWin,
			       cls, 3);
	XLIB_EPILOGUE (dmxScreen);
    }
    else
    {
	dmxScreen->rootEventMask |= DMX_KEYBOARD_EVENT_MASK;

	XLIB_PROLOGUE (dmxScreen);
	XSelectInput (dmxScreen->beDisplay, dmxScreen->rootWin,
		      dmxScreen->rootEventMask);
	XLIB_EPILOGUE (dmxScreen);
    }

    return Success;
}

static void
dmxKeyboardOff (DeviceIntPtr pDevice)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;

    if (pDevPriv->deviceId >= 0)
    {
	if (pDevPriv->device)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    XCloseDevice (dmxScreen->beDisplay, pDevPriv->device);
	    XLIB_EPILOGUE (dmxScreen);

	    pDevPriv->device = NULL;
	}
    }
    else
    {
	dmxScreen->rootEventMask &= ~DMX_KEYBOARD_EVENT_MASK;

	if (dmxScreen->scrnWin)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    XSelectInput (dmxScreen->beDisplay, dmxScreen->rootWin,
			  dmxScreen->rootEventMask);
	    XLIB_EPILOGUE (dmxScreen);
	}
    }

    if (pDevPriv->keySyms.map)
	xfree (pDevPriv->keySyms.map);

    xfree (pDevPriv->keycode);
}

static int
dmxKeyboardProc (DeviceIntPtr pDevice,
		 int          what)
{
    DevicePtr            pDev = (DevicePtr) pDevice;
    CARD8                modMap[MAP_LENGTH];
    KeySymsRec           keySyms;

#ifdef XKB
    XkbComponentNamesRec names;
#endif

    switch (what) {
    case DEVICE_INIT:
        keySyms.minKeyCode = 8;
        keySyms.maxKeyCode = 255;
        keySyms.mapWidth = 4;
        keySyms.map = (KeySym *) Xcalloc (sizeof (KeySym),
					  (keySyms.maxKeyCode -
					   keySyms.minKeyCode + 1) *
					  keySyms.mapWidth);
        if (!keySyms.map)
	{
            ErrorF ("[dmx] Couldn't allocate keymap\n");
            return BadAlloc;
        }

        bzero ((char *) modMap, MAP_LENGTH);

#ifdef XKB
        if (!noXkbExtension)
	{
            bzero (&names, sizeof (names));

            XkbSetRulesDflts ("base", "pc105", "us", NULL, NULL);
            XkbInitKeyboardDeviceStruct (pDevice,
					 &names,
					 &keySyms,
					 modMap,
					 dmxKeyboardBell,
					 (KbdCtrlProcPtr) NoopDDA);
        }
        else
#endif
        {
            /* FIXME Our keymap here isn't exactly useful. */
            InitKeyboardDeviceStruct (pDev,
				      &keySyms,
				      modMap,
                                      dmxKeyboardBell,
				      (KbdCtrlProcPtr) NoopDDA);
        }

        xfree (keySyms.map);
	break;
    case DEVICE_ON:
	pDev->on = 1;
	dmxKeyboardOn (pDevice);
	break;
    case DEVICE_OFF:
	pDev->on = 0;
	dmxKeyboardOff (pDevice);
	break;
    case DEVICE_CLOSE:
	if (pDev->on)
	{
	    pDev->on = 0;
	    dmxKeyboardOff (pDevice);
	}
	break;
    }

    return Success;
}

static int
dmxPointerOn (DeviceIntPtr pDevice)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;

    if (pDevPriv->deviceId >= 0)
    {
	XEventClass cls[5];
	int         type;

	pDevPriv->device = XOpenDevice (dmxScreen->beDisplay,
					pDevPriv->deviceId);
	if (!pDevPriv->device)
	{
	    dmxLog (dmxWarning, "Cannot open %s device (id=%d) on %s\n",
		    pDevice->name, pDevPriv->deviceId, dmxScreen->name);
	    return -1;
	}

	DeviceMotionNotify (pDevPriv->device, type, cls[0]);
	DeviceButtonPress (pDevPriv->device, type, cls[1]);
	DeviceButtonRelease (pDevPriv->device, type, cls[2]);
	DeviceButtonPressGrab (pDevPriv->device, type, cls[3]);
	DeviceStateNotify (pDevPriv->device, type, cls[4]);

	XLIB_PROLOGUE (dmxScreen);
	XSelectExtensionEvent (dmxScreen->beDisplay, dmxScreen->rootWin,
			       cls, 5);
	XLIB_EPILOGUE (dmxScreen);
    }
    else
    {
	dmxScreen->rootEventMask |= DMX_POINTER_EVENT_MASK;

	XLIB_PROLOGUE (dmxScreen);
	XSelectInput (dmxScreen->beDisplay, dmxScreen->rootWin,
		      dmxScreen->rootEventMask);
	XLIB_EPILOGUE (dmxScreen);
    }
    
    return -1;
}

static void
dmxPointerOff (DeviceIntPtr pDevice)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
    DMXScreenInfo    *dmxScreen = (DMXScreenInfo *) pDevPriv->dmxInput;

    if (pDevPriv->deviceId >= 0)
    {
	if (pDevPriv->device)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    XCloseDevice (dmxScreen->beDisplay, pDevPriv->device);
	    XLIB_EPILOGUE (dmxScreen);

	    pDevPriv->device = NULL;
	}
    }
    else
    {
	dmxScreen->rootEventMask &= ~DMX_POINTER_EVENT_MASK;
	
	if (dmxScreen->scrnWin)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    XSelectInput (dmxScreen->beDisplay, dmxScreen->rootWin,
			  dmxScreen->rootEventMask);
	    XLIB_EPILOGUE (dmxScreen);
	}
    }
}

static int
dmxPointerProc (DeviceIntPtr pDevice,
		int          what)
{
    DevicePtr pDev = (DevicePtr) pDevice;
    BYTE      map[33];
    int       i;

    switch (what) {
    case DEVICE_INIT:
	for (i = 1; i <= 32; i++)
            map[i] = i;

        InitPointerDeviceStruct (pDev,
				 map, 32,
				 (PtrCtrlProcPtr) NoopDDA,
				 GetMotionHistorySize (), 2);

	pDevice->valuator->axisVal[0] = screenInfo.screens[0]->width / 2;
        pDevice->last.valuators[0] = pDevice->valuator->axisVal[0];
        pDevice->valuator->axisVal[1] = screenInfo.screens[0]->height / 2;
        pDevice->last.valuators[1] = pDevice->valuator->axisVal[1];
	break;
    case DEVICE_ON:
	pDev->on = 1;
	dmxPointerOn (pDevice);
	break;
    case DEVICE_OFF:
	pDev->on = 0;
	dmxPointerOff (pDevice);
	break;
    case DEVICE_CLOSE:
	if (pDev->on)
	{
	    pDev->on = 0;
	    dmxPointerOff (pDevice);
	}
	break;
    }

    return Success;
}

static char *
dmxMakeUniqueDeviceName (DMXInputInfo *dmxInput,
			 const char   *deviceName)
{
    DMXScreenInfo *dmxScreen = (DMXScreenInfo *) dmxInput;
    char          *name;
    int           i, n = 2;

#define LEN 32

    name = xalloc (strlen (dmxScreen->name) + strlen (deviceName) + LEN);
    if (!name)
	return NULL;
	
    sprintf (name, "%s's %s", dmxScreen->name, deviceName);

    do {
	for (i = 0; i < dmxInput->numDevs; i++)
	{
	    if (strcmp (dmxInput->devs[i]->name, name) == 0)
	    {
		sprintf (name, "%s's %s%d", dmxScreen->name, deviceName, n++);
		break;
	    }
	}
    } while (i < dmxInput->numDevs);

    return name;
}

static DeviceIntPtr
dmxAddInputDevice (DMXInputInfo *dmxInput,
		   DeviceProc   deviceProc,
		   const char   *deviceName,
		   int          deviceId,
		   int          masterId,
		   Bool         (*EventCheck) (DeviceIntPtr,
					       xcb_generic_event_t *),
		   Bool         (*ReplyCheck) (DeviceIntPtr,
					       unsigned int,
					       xcb_generic_reply_t *))
{
    DeviceIntPtr pDevice, *devs;
    char         *name;

    devs = xrealloc (dmxInput->devs, sizeof (DeviceIntPtr) *
		     (dmxInput->numDevs + 1));
    if (!devs)
	return NULL;

    dmxInput->devs = devs;

    name = dmxMakeUniqueDeviceName (dmxInput, deviceName);
    if (!name)
	return NULL;

    pDevice = AddInputDevice (serverClient, deviceProc, TRUE);
    if (pDevice)
    {
	dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDevice);
	
	pDevice->name = name;

	pDevPriv->dmxInput   = dmxInput;
	pDevPriv->deviceId   = deviceId;
	pDevPriv->masterId   = masterId;
	pDevPriv->device     = NULL;
	pDevPriv->fakeGrab   = xFalse;
	pDevPriv->EventCheck = EventCheck;
	pDevPriv->ReplyCheck = ReplyCheck;

	memset (pDevPriv->state, 0, sizeof (pDevPriv->state));
	memset (pDevPriv->keysbuttons, 0, sizeof (pDevPriv->keysbuttons));

	pDevPriv->grab.sequence = 0;

	RegisterOtherDevice (pDevice);

	dmxInput->devs[dmxInput->numDevs] = pDevice;
	dmxInput->numDevs++;

	dmxLogInput (dmxInput, "Added extension device called %s\n", name);
    }
    else
    {
	xfree (name);
    }

    return pDevice;
}

static XID
dmxGetMasterDevice (XDeviceInfo *device)
{
    XAttachInfoPtr ai;
    int            i;

    for (i = 0, ai = (XAttachInfoPtr) device->inputclassinfo;
	 i < device->num_classes;
	 ai = (XAttachInfoPtr) ((char *) ai + ai->length), i++)
	if (ai->class == AttachClass)
	    return ai->attached;

    return -1;
}

static Bool
dmxInputAddExtensionDevices (DMXInputInfo *dmxInput)
{
    DMXScreenInfo     *dmxScreen = (DMXScreenInfo *) dmxInput;
    XExtensionVersion *ext;
    XDeviceInfo       *devices;
    int               num;
    int               i;

    ext = XQueryInputVersion (dmxScreen->beDisplay, XI_2_Major, XI_2_Minor);
    if (!ext || ext == (XExtensionVersion *) NoSuchExtension)
    {
        dmxLogInput (dmxInput, "%s is not available\n", INAME);
	return FALSE;
    }

    /* Only use XInput Extension if 2.0 or greater */
    if (ext->major_version < 2)
    {
        dmxLogInput (dmxInput, "%s version %d.%d is too old\n",
		     INAME, ext->major_version, ext->minor_version);
	XFree (ext);
	return FALSE;
    }

    XQueryExtension (dmxScreen->beDisplay,
		     INAME,
		     &i,
		     &dmxInput->eventBase,
		     &i);
    
    dmxLogInput (dmxInput, "Locating devices on %s (%s version %d.%d)\n",
		 dmxScreen->name, INAME,
		 ext->major_version, ext->minor_version);

    XFree (ext);

    devices = XListInputDevices (dmxScreen->beDisplay, &num);
    if (devices)
    {
	for (i = 0; i < num; i++)
	{
	    switch (devices[i].use) {
	    case IsXKeyboard:
		dmxAddInputDevice (dmxInput,
				   dmxKeyboardProc,
				   devices[i].name,
				   devices[i].id,
				   dmxGetMasterDevice (&devices[i]),
				   dmxDeviceKeyboardEventCheck,
				   dmxDeviceKeyboardReplyCheck);
		break;
	    case IsXPointer:
		dmxAddInputDevice (dmxInput,
				   dmxPointerProc,
				   devices[i].name,
				   devices[i].id,
				   dmxGetMasterDevice (&devices[i]),
				   dmxDevicePointerEventCheck,
				   dmxDevicePointerReplyCheck);
		break;
	    }
	}

	XFreeDeviceList (devices);
    }

    return TRUE;
}

static void
dmxInputAddDevices (DMXInputInfo *dmxInput)
{
    if (!dmxInputAddExtensionDevices (dmxInput))
    {
	dmxLogInput (dmxInput, "Using core devices from %s\n",
		     ((DMXScreenInfo *) dmxInput)->name);

	dmxAddInputDevice (dmxInput,
			   dmxKeyboardProc, "core keyboard",
			   -1, -1,
			   dmxDeviceKeyboardEventCheck,
			   dmxDeviceKeyboardReplyCheck);
	dmxAddInputDevice (dmxInput,
			   dmxPointerProc, "core pointer",
			   -1, -1,
			   dmxDevicePointerEventCheck,
			   dmxDevicePointerReplyCheck);
    }
}

static void
dmxInputRemoveDevices (DMXInputInfo *dmxInput)
{
    dmxLogInput (dmxInput, "Removing devices from %s\n",
		 ((DMXScreenInfo *) dmxInput)->name);

    while (dmxInput->numDevs)
	DeleteInputDeviceRequest (*dmxInput->devs);
}

int
dmxInputEnable (DMXInputInfo *dmxInput)
{
    int i;
    
    for (i = 0; i < dmxInput->numDevs; i++)
    {
	dmxLogInput (dmxInput, "Activate device id %d: %s\n",
		     dmxInput->devs[i]->id, dmxInput->devs[i]->name);

        if (ActivateDevice (dmxInput->devs[i]) != Success)
	    return BadImplementation;
	
	if (!EnableDevice (dmxInput->devs[i]))
	    return BadImplementation;
    }

    return 0;
}

int
dmxInputDisable (DMXInputInfo *dmxInput)
{
    char state[32];
    int  i;

    memset (state, 0, sizeof (state));
    
    for (i = 0; i < dmxInput->numDevs; i++)
    {
        dmxLogInput (dmxInput, "Disable device id %d: %s\n",
		     dmxInput->devs[i]->id, dmxInput->devs[i]->name);

	if (dmxInput->devs[i]->key)
	    dmxUpdateKeyState (dmxInput->devs[i], state);
	else if (dmxInput->devs[i]->button)
	    dmxUpdateButtonState (dmxInput->devs[i], state);

        DisableDevice (dmxInput->devs[i]);
    }

    return 0;
}

int
dmxInputAttach (DMXInputInfo *dmxInput)
{
    dmxInputAddDevices (dmxInput);

    return 0;
}

int
dmxInputDetach (DMXInputInfo *dmxInput)
{
    ProcessInputEvents ();

    dmxInputRemoveDevices (dmxInput);

    return 0;
}

void
dmxInputInit (DMXInputInfo *dmxInput)
{
    dmxInput->devs      = NULL;
    dmxInput->numDevs   = 0;
    dmxInput->eventBase = 0;
}

void
dmxInputFini (DMXInputInfo *dmxInput)
{
    if (dmxInput->devs)
	xfree (dmxInput->devs);
}

Bool
LegalModifier (unsigned int key,
	       DeviceIntPtr pDev)
{
    return TRUE;
}

void
InitInput (int argc, char **argv)
{
    int i;

    GetEventList (&dmxEvents);

    for (i = 0; i < dmxNumScreens; i++)
	if (dmxScreens[i].beDisplay && !dmxScreens[i].virtualFb)
	    dmxInputAddDevices (&dmxScreens[i].input);

    mieqInit ();
}

void
ProcessInputEvents (void)
{
    mieqProcessInputEvents ();
}

void
CloseInputDevice (DeviceIntPtr pDevice,
		  ClientPtr    client)
{
}

void
AddOtherInputDevices (void)
{
}

void
OpenInputDevice (DeviceIntPtr pDevice,
		 ClientPtr    client,
		 int          *status)
{
    *status = XaceHook (XACE_DEVICE_ACCESS, client, pDevice, DixReadAccess);
}

int
SetDeviceMode (ClientPtr    client,
	       DeviceIntPtr pDevice,
	       int          mode)
{
    return BadMatch;
}

int
SetDeviceValuators (ClientPtr    client,
		    DeviceIntPtr pDevice,
		    int          *valuators,
		    int          first_valuator,
		    int          num_valuators)
{
    return BadMatch;
}

int
ChangeDeviceControl (ClientPtr    client,
		     DeviceIntPtr pDevice,
		     xDeviceCtl   *control)
{
    return BadMatch;
}

int
NewInputDeviceRequest (InputOption  *options,
		       DeviceIntPtr *ppDevice)
{
    return BadRequest;
}

void
DeleteInputDeviceRequest (DeviceIntPtr pDevice)
{
    int i, j;

    RemoveDevice (pDevice);

    for (i = 0; i < dmxNumScreens; i++)
    {
	for (j = 0; j < dmxScreens[i].input.numDevs; j++)
	{
	    if (dmxScreens[i].input.devs[j] == pDevice)
	    {
		for (; j < dmxScreens[i].input.numDevs - 1; j++)
		    dmxScreens[i].input.devs[j] =
			dmxScreens[i].input.devs[j + 1];

		dmxScreens[i].input.numDevs--;
		return;
	    }
	}
    }
}

void
DDXRingBell (int volume,
	     int pitch,
	     int duration)
{
}
