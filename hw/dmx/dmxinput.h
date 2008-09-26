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

#ifndef DMXINPUT_H
#define DMXINPUT_H

extern DevPrivateKey dmxDevicePrivateKey;

/** Device private area. */
typedef struct _dmxDevicePriv {
    DMXInputInfo             *dmxInput;
    long                     deviceId;
    long                     masterId;
    XDevice                  *device;
    char		     state[32];
    char		     keysbuttons[32];
    KeySymsRec               keySyms;
    KeyCode                  *keycode;
    xcb_void_cookie_t        grab;

    Bool (*EventCheck) (DeviceIntPtr, xcb_generic_event_t *);
    Bool (*ReplyCheck) (DeviceIntPtr, unsigned int, xcb_generic_reply_t *);

    void (*ActivateGrab)   (DeviceIntPtr, GrabPtr, TimeStamp, Bool);
    void (*DeactivateGrab) (DeviceIntPtr);
} dmxDevicePrivRec, *dmxDevicePrivPtr;

#define DMX_GET_DEVICE_PRIV(_pDev)				\
    ((dmxDevicePrivPtr)dixLookupPrivate(&(_pDev)->devPrivates,	\
					dmxDevicePrivateKey))

void
dmxInputInit (DMXInputInfo *dmxInput);

void
dmxInputLogDevices (void);

Bool
dmxInputEventCheck (DMXInputInfo        *dmxInput,
		    xcb_generic_event_t *event);

Bool
dmxInputReplyCheck (DMXInputInfo        *dmxInput,
		    unsigned int        request,
		    xcb_generic_reply_t *reply);

void
dmxInputGrabButton (DMXInputInfo *dmxInput,
		    DeviceIntPtr pDevice,
		    DeviceIntPtr pModDevice,
		    WindowPtr    pWindow,
		    WindowPtr    pConfineTo,
		    int	         button,
		    int	         modifiers,
		    CursorPtr    pCursor);

void
dmxInputUngrabButton (DMXInputInfo *dmxInput,
		      DeviceIntPtr pDevice,
		      DeviceIntPtr pModDevice,
		      WindowPtr    pWindow,
		      int	   button,
		      int	   modifiers);

void
dmxInputGrabPointer (DMXInputInfo *dmxInput,
		     DeviceIntPtr pDevice,
		     WindowPtr    pWindow,
		     WindowPtr    pConfineTo,
		     CursorPtr    pCursor);

void
dmxInputUngrabPointer (DMXInputInfo *dmxInput,
		       DeviceIntPtr pDevice,
		       WindowPtr    pWindow);

int
dmxInputEnable (DMXInputInfo *dmxInput);

int
dmxInputDisable (DMXInputInfo *dmxInput);

int
dmxInputAttach (DMXInputInfo *dmxInput);

int
dmxInputDetach (DMXInputInfo *dmxInput);

void
dmxInputInit (DMXInputInfo *dmxInput);

void
dmxInputFini (DMXInputInfo *dmxInput);

#endif /* DMXINPUT_H */
