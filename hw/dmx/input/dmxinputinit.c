/*
 * Copyright 2002-2003 Red Hat Inc., Durham, North Carolina.
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
 *   Rickard E. (Rik) Faith <faith@redhat.com>
 *
 */

/** \file
 * This file provides generic input support.  Functions here set up
 * input and lead to the calling of low-level device drivers for
 * input. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#define DMX_WINDOW_DEBUG 0

#include "dmxinputinit.h"
#include "dmxextension.h"       /* For dmxInputCount */

#include "dmxbackend.h"
#include "dmxcommon.h"
#include "dmxprop.h"
#include "dmxcursor.h"

#include "inputstr.h"
#include "input.h"
#include "mipointer.h"
#include "windowstr.h"
#include "mi.h"

#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include "exevents.h"
#define EXTENSION_PROC_ARGS void *
#include "extinit.h"

/* From XI.h */
#ifndef Relative
#define Relative 0
#endif
#ifndef Absolute
#define Absolute 1
#endif

DMXLocalInputInfoPtr dmxLocalCorePointer, dmxLocalCoreKeyboard;

static DMXLocalInputInfoRec DMXBackendMou = {
    "backend-mou", DMX_LOCAL_MOUSE, DMX_LOCAL_TYPE_BACKEND, 2,
    dmxBackendCreatePrivate, dmxBackendDestroyPrivate,
    dmxBackendInit, NULL, dmxBackendLateReInit, dmxBackendMouGetInfo,
    dmxCommonMouOn, dmxCommonMouOff, dmxBackendUpdatePosition,
    NULL, NULL, NULL,
    NULL, dmxBackendProcessInput, dmxBackendFunctions, NULL,
    dmxBackendPointerEventCheck, dmxBackendPointerReplyCheck,
    dmxBackendGrabButton, dmxBackendUngrabButton,
    dmxBackendGrabPointer, dmxBackendUngrabPointer,
    dmxCommonMouCtrl
};

static DMXLocalInputInfoRec DMXBackendKbd = {
    "backend-kbd", DMX_LOCAL_KEYBOARD, DMX_LOCAL_TYPE_BACKEND,
    1, /* With backend-mou or console-mou */
    dmxCommonCopyPrivate, NULL,
    dmxBackendInit, NULL, NULL, dmxBackendKbdGetInfo,
    dmxCommonKbdOn, dmxBackendKbdOff, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    dmxBackendKeyboardEventCheck, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, dmxCommonKbdCtrl, dmxCommonKbdBell
};

#if 11 /*BP*/
void
DDXRingBell(int volume, int pitch, int duration)
{
   /* NO-OP */
}

/* taken from kdrive/src/kinput.c: */
static void
dmxKbdCtrl (DeviceIntPtr pDevice, KeybdCtrl *ctrl)
{
#if 0
    KdKeyboardInfo *ki;

    for (ki = kdKeyboards; ki; ki = ki->next) {
        if (ki->dixdev && ki->dixdev->id == pDevice->id)
            break;
    }

    if (!ki || !ki->dixdev || ki->dixdev->id != pDevice->id || !ki->driver)
        return;

    KdSetLeds(ki, ctrl->leds);
    ki->bellPitch = ctrl->bell_pitch;
    ki->bellDuration = ctrl->bell_duration; 
#endif
}

/* taken from kdrive/src/kinput.c: */
static void
dmxBell(int volume, DeviceIntPtr pDev, pointer arg, int something)
{
#if 0
    KeybdCtrl *ctrl = arg;
    KdKeyboardInfo *ki = NULL;
    
    for (ki = kdKeyboards; ki; ki = ki->next) {
        if (ki->dixdev && ki->dixdev->id == pDev->id)
            break;
    }

    if (!ki || !ki->dixdev || ki->dixdev->id != pDev->id || !ki->driver)
        return;
    
    KdRingBell(ki, volume, ctrl->bell_pitch, ctrl->bell_duration);
#endif
}

#endif /*BP*/

static void _dmxChangePointerControl(DMXLocalInputInfoPtr dmxLocal,
                                     PtrCtrl *ctrl)
{
    if (!dmxLocal) return;
    dmxLocal->mctrl = *ctrl;
    if (dmxLocal->mCtrl) dmxLocal->mCtrl(&dmxLocal->pDevice->public, ctrl);
}

/** Change the pointer control information for the \a pDevice.  If the
 * device sends core events, then also change the control information
 * for all of the pointer devices that send core events. */
void dmxChangePointerControl(DeviceIntPtr pDevice, PtrCtrl *ctrl)
{
    GETDMXLOCALFROMPDEVICE;
    int i, j;

    if (dmxLocal->sendsCore) {       /* Do for all core devices */
        for (i = 0; i < dmxNumInputs; i++) {
            DMXInputInfo *dmxInput = &dmxInputs[i];
            if (dmxInput->detached) continue;
            for (j = 0; j < dmxInput->numDevs; j++)
                if (dmxInput->devs[j]->sendsCore)
                    _dmxChangePointerControl(dmxInput->devs[j], ctrl);
        }
    } else {                    /* Do for this device only */
        _dmxChangePointerControl(dmxLocal, ctrl);
    }
}

static void _dmxKeyboardKbdCtrlProc(DMXLocalInputInfoPtr dmxLocal,
                                    KeybdCtrl *ctrl)
{
    dmxLocal->kctrl = *ctrl;
    if (dmxLocal->kCtrl) {
        dmxLocal->kCtrl(&dmxLocal->pDevice->public, ctrl);
#ifdef XKB
        if (!noXkbExtension && dmxLocal->pDevice->kbdfeed) {
            XkbEventCauseRec cause;
            XkbSetCauseUnknown(&cause);
            /* Generate XKB events, as necessary */
            XkbUpdateIndicators(dmxLocal->pDevice, XkbAllIndicatorsMask, False,
                                NULL, &cause);
        }
#endif
    }
}


/** Change the keyboard control information for the \a pDevice.  If the
 * device sends core events, then also change the control information
 * for all of the keyboard devices that send core events. */
void dmxKeyboardKbdCtrlProc(DeviceIntPtr pDevice, KeybdCtrl *ctrl)
{
    GETDMXLOCALFROMPDEVICE;
    int i, j;

    if (dmxLocal->sendsCore) {       /* Do for all core devices */
        for (i = 0; i < dmxNumInputs; i++) {
            DMXInputInfo *dmxInput = &dmxInputs[i];
            if (dmxInput->detached) continue;
            for (j = 0; j < dmxInput->numDevs; j++)
                if (dmxInput->devs[j]->sendsCore)
                    _dmxKeyboardKbdCtrlProc(dmxInput->devs[j], ctrl);
        }
    } else {                    /* Do for this device only */
        _dmxKeyboardKbdCtrlProc(dmxLocal, ctrl);
    }
}

static void _dmxKeyboardBellProc(DMXLocalInputInfoPtr dmxLocal, int percent)
{
    if (dmxLocal->kBell) dmxLocal->kBell(&dmxLocal->pDevice->public,
                                         percent,
                                         dmxLocal->kctrl.bell,
                                         dmxLocal->kctrl.bell_pitch,
                                         dmxLocal->kctrl.bell_duration);
}

/** Sound the bell on the device.  If the device send core events, then
 * sound the bell on all of the devices that send core events. */
void dmxKeyboardBellProc(int percent, DeviceIntPtr pDevice,
                         pointer ctrl, int unknown)
{
    GETDMXLOCALFROMPDEVICE;
    int i, j;

    if (dmxLocal->sendsCore) {       /* Do for all core devices */
        for (i = 0; i < dmxNumInputs; i++) {
            DMXInputInfo *dmxInput = &dmxInputs[i];
            if (dmxInput->detached) continue;
            for (j = 0; j < dmxInput->numDevs; j++)
                if (dmxInput->devs[j]->sendsCore)
                    _dmxKeyboardBellProc(dmxInput->devs[j], percent);
        }
    } else {                    /* Do for this device only */
        _dmxKeyboardBellProc(dmxLocal, percent);
    }
}

#ifdef XKB
static void dmxKeyboardFreeNames(XkbComponentNamesPtr names)
{
    if (names->keymap)   XFree(names->keymap);
    if (names->keycodes) XFree(names->keycodes);
    if (names->types)    XFree(names->types);
    if (names->compat)   XFree(names->compat);
    if (names->symbols)  XFree(names->symbols);
    if (names->geometry) XFree(names->geometry);
}
#endif


static int dmxKeyboardOn(DeviceIntPtr pDevice, DMXLocalInitInfo *info)
{
    DevicePtr pDev = &pDevice->public;

#ifdef XKB
    if (noXkbExtension) {
#endif
        if (!InitKeyboardDeviceStruct(pDev, &info->keySyms, info->modMap,
                                      dmxKeyboardBellProc,
                                      dmxKeyboardKbdCtrlProc))
            return BadImplementation;
#ifdef XKB
    } else {
        XkbInitKeyboardDeviceStruct(pDevice,
                                    &info->names,
                                    &info->keySyms,
                                    info->modMap,
                                    dmxKeyboardBellProc,
                                    dmxKeyboardKbdCtrlProc);
    }
    if (info->freenames) dmxKeyboardFreeNames(&info->names);
#endif

    return Success;
}

    
static int dmxDeviceOnOff(DeviceIntPtr pDevice, int what)
{
    GETDMXINPUTFROMPDEVICE;
    DMXLocalInitInfo info;
    int              i;
    
    if (dmxInput->detached) return Success;

    memset(&info, 0, sizeof(info));
    switch (what) {
    case DEVICE_INIT:
        if (dmxLocal->init)
            dmxLocal->init(pDev);
        if (dmxLocal->get_info)
            dmxLocal->get_info(pDev, &info);
        if (info.keyboard) {    /* XKEYBOARD makes this a special case */
            dmxKeyboardOn(pDevice, &info);
            break;
        }
        if (info.keyClass) {
            DevicePtr pDev = (DevicePtr) pDevice;
            InitKeyboardDeviceStruct(pDev,
                                     &info.keySyms,
                                     info.modMap,
                                     dmxBell, dmxKbdCtrl);
        }
	if (info.buttonClass) {
            InitButtonClassDeviceStruct(pDevice, info.numButtons, info.map);
        }
        if (info.valuatorClass) {
            if (info.numRelAxes) {
                InitValuatorClassDeviceStruct(pDevice, info.numRelAxes,
                                              GetMaximumEventsNum(),
                                              Relative);
                for (i = 0; i < info.numRelAxes; i++)
                    InitValuatorAxisStruct(pDevice, i, info.minval[0],
                                           info.maxval[0], info.res[0],
                                           info.minres[0], info.maxres[0]);
            } else if (info.numRelAxes) {
                InitValuatorClassDeviceStruct(pDevice, info.numRelAxes,
					      GetMaximumEventsNum(),
                                              Relative);
                for (i = 0; i < info.numRelAxes; i++)
                    InitValuatorAxisStruct(pDevice, i, info.minval[0],
                                           info.maxval[0], info.res[0],
                                           info.minres[0], info.maxres[0]);
            } else if (info.numAbsAxes) {
                InitValuatorClassDeviceStruct(pDevice, info.numAbsAxes,
                                              GetMaximumEventsNum(),
                                              Absolute);
                for (i = 0; i < info.numAbsAxes; i++)
                    InitValuatorAxisStruct(pDevice, i+info.numRelAxes,
                                           info.minval[i+1], info.maxval[i+1],
                                           info.res[i+1], info.minres[i+1],
                                           info.maxres[i+1]);
            }
        }
        if (info.focusClass)       InitFocusClassDeviceStruct(pDevice);
        if (info.proximityClass)   InitProximityClassDeviceStruct(pDevice);
        if (info.ptrFeedbackClass)
            InitPtrFeedbackClassDeviceStruct(pDevice, dmxChangePointerControl);
        if (info.kbdFeedbackClass)
            InitKbdFeedbackClassDeviceStruct(pDevice, dmxKeyboardBellProc,
                                             dmxKeyboardKbdCtrlProc);
        if (info.intFeedbackClass || info.strFeedbackClass)
            dmxLog(dmxWarning,
                   "Integer and string feedback not supported for %s\n",
                   pDevice->name);
        if (!info.keyboard && (info.ledFeedbackClass || info.belFeedbackClass))
            dmxLog(dmxWarning,
                   "Led and bel feedback not supported for non-keyboard %s\n",
                   pDevice->name);
        break;
    case DEVICE_ON:
	pDev->on = TRUE;
        break;
    case DEVICE_OFF:
    case DEVICE_CLOSE:
            /* This can get called twice consecutively: once for a
             * detached screen (DEVICE_OFF), and then again at server
             * generation time (DEVICE_CLOSE). */
        if (pDev->on) {
            if (dmxLocal->off) dmxLocal->off(pDev);
            pDev->on = FALSE;
        }
	dmxInput->detached = True;
        break;
    }
    if (info.keySyms.map && info.freemap) {
        XFree(info.keySyms.map);
        info.keySyms.map = NULL;
    }
#ifdef XKB
    if (info.xkb) XkbFreeKeyboard(info.xkb, 0, True);
#endif
    return Success;
}

static Bool dmxScreenEventCheck(DMXInputInfo        *dmxInput,
				xcb_generic_event_t *event)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DevicePtr pDev = &dmxInput->devs[i]->pDevice->public;

        if (!dmxInput->devs[i]->event_check)
	    continue;

	if ((*dmxInput->devs[i]->event_check) (pDev, event))
	    return TRUE;
    }

    return FALSE;
}

static Bool dmxScreenReplyCheck(DMXInputInfo        *dmxInput,
				unsigned int        request,
				xcb_generic_reply_t *reply)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DevicePtr pDev = &dmxInput->devs[i]->pDevice->public;

        if (!dmxInput->devs[i]->reply_check)
	    continue;

	if ((*dmxInput->devs[i]->reply_check) (pDev, request, reply))
	    return TRUE;
    }

    return FALSE;
}

static void dmxProcessInputEvents(DMXInputInfo *dmxInput)
{
    int i;

#if 00 /*BP*/
    miPointerUpdate();
#endif
    if (dmxInput->detached)
        return;
    for (i = 0; i < dmxInput->numDevs; i += dmxInput->devs[i]->binding)
        if (dmxInput->devs[i]->process_input) {
#if 11 /*BP*/
            //miPointerUpdateSprite(dmxInput->devs[i]->pDevice);
#endif
            dmxInput->devs[i]->process_input(dmxInput->devs[i]->private);
        }
}

static void dmxGrabButton(DMXInputInfo *dmxInput,
			  DeviceIntPtr pDevice,
			  DeviceIntPtr pModDevice,
			  WindowPtr    pWindow,
			  WindowPtr    pConfineTo,
			  int	       button,
			  int	       modifiers,
			  CursorPtr    pCursor)
{
    int i, j;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DevicePtr pDev = &dmxInput->devs[i]->pDevice->public;

	if (dmxInput->devs[i]->pDevice->u.master != pDevice)
	    continue;

	if (!dmxInput->devs[i]->grab_button)
	    continue;

	if (modifiers & AnyModifier)
	{
	    (*dmxInput->devs[i]->grab_button) (pDev,
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
		DevicePtr pModDev = &dmxInput->devs[j]->pDevice->public;

		if (dmxInput->devs[j]->pDevice->u.master == pModDevice)
		    (*dmxInput->devs[i]->grab_button) (pDev,
						       pModDev,
						       pWindow,
						       pConfineTo,
						       button,
						       modifiers,
						       pCursor);
	    }
	}
    }
}

static void dmxUngrabButton(DMXInputInfo *dmxInput,
			    DeviceIntPtr pDevice,
			    DeviceIntPtr pModDevice,
			    WindowPtr    pWindow,
			    int	         button,
			    int	         modifiers)
{
    int i, j;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DevicePtr pDev = &dmxInput->devs[i]->pDevice->public;

	if (dmxInput->devs[i]->pDevice->u.master != pDevice)
	    continue;

	if (!dmxInput->devs[i]->ungrab_button)
	    continue;

	if (modifiers == AnyModifier)
	{
	    (*dmxInput->devs[i]->ungrab_button) (pDev,
						 NULL,
						 pWindow,
						 button,
						 modifiers);
	}
	else
	{
	    for (j = 0; j < dmxInput->numDevs; j++)
	    {
		DevicePtr pModDev = &dmxInput->devs[j]->pDevice->public;

		if (dmxInput->devs[j]->pDevice->u.master == pModDevice)
		    (*dmxInput->devs[i]->ungrab_button) (pDev,
							 pModDev,
							 pWindow,
							 button,
							 modifiers);
	    }
	}
    }
}

static void dmxGrabPointer (DMXInputInfo *dmxInput,
			    DeviceIntPtr pDevice,
			    WindowPtr    pWindow,
			    WindowPtr    pConfineTo,
			    CursorPtr    pCursor)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DevicePtr pDev = &dmxInput->devs[i]->pDevice->public;

	if (dmxInput->devs[i]->pDevice->u.master != pDevice)
	    continue;

	if (!dmxInput->devs[i]->grab_pointer)
	    continue;

	(*dmxInput->devs[i]->grab_pointer) (pDev,
					    pWindow,
					    pConfineTo,
					    pCursor);
    }
}

static void dmxUngrabPointer (DMXInputInfo *dmxInput,
			      DeviceIntPtr pDevice,
			      WindowPtr    pWindow)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++)
    {
	DevicePtr pDev = &dmxInput->devs[i]->pDevice->public;

	if (dmxInput->devs[i]->pDevice->u.master != pDevice)
	    continue;

	if (!dmxInput->devs[i]->ungrab_pointer)
	    continue;

	(*dmxInput->devs[i]->ungrab_pointer) (pDev, pWindow);
    }
}

static char *dmxMakeUniqueDeviceName(DMXLocalInputInfoPtr dmxLocal)
{
    DMXInputInfo *dmxInput = &dmxInputs[dmxLocal->inputIdx];
    char         *buf;

    if (dmxLocal->deviceName)
    {
	buf = xalloc (strlen (dmxScreens[dmxInput->scrnIdx].name) +
		      strlen (dmxLocal->deviceName) + 4);
	if (buf)
	{
	    sprintf (buf, "%s's %s", dmxScreens[dmxInput->scrnIdx].name,
		     dmxLocal->deviceName);
	    return buf;
	}
    }

#define LEN 32

    buf = xalloc (strlen (dmxScreens[dmxInput->scrnIdx].name) + LEN);
    if (buf)
    {
	switch (dmxLocal->type) {
	case DMX_LOCAL_KEYBOARD:
	    if (dmxInput->k)
		sprintf (buf, "%s's keyboard%d",
			 dmxScreens[dmxInput->scrnIdx].name, dmxInput->k);
	    else
		sprintf (buf, "%s's keyboard",
			 dmxScreens[dmxInput->scrnIdx].name);
	    dmxInput->k++;
	    break;
	case DMX_LOCAL_MOUSE:
	    if (dmxInput->m)
		sprintf (buf, "%s's pointer%d",
			 dmxScreens[dmxInput->scrnIdx].name, dmxInput->m);
	    else
		sprintf (buf, "%s's pointer",
			 dmxScreens[dmxInput->scrnIdx].name);
	    dmxInput->m++;
	    break;
	default:
	    if (dmxInput->o)
		sprintf (buf, "%s's input device%d",
			 dmxScreens[dmxInput->scrnIdx].name, dmxInput->o);
	    else
		sprintf (buf, "%s's input device",
			 dmxScreens[dmxInput->scrnIdx].name);
	    dmxInput->o++;
	    break;
	}
    }

    return buf;
}

static DeviceIntPtr dmxAddDevice(DMXLocalInputInfoPtr dmxLocal)
{
    DeviceIntPtr pDevice;
    Atom         atom;
    char         *devname;
    DMXInputInfo *dmxInput;

    if (!dmxLocal)
        return NULL;
    dmxInput = &dmxInputs[dmxLocal->inputIdx];

    pDevice = AddInputDevice (serverClient, dmxDeviceOnOff, TRUE);
    if (!pDevice) {
        dmxLog(dmxError, "Too many devices -- cannot add device %s\n",
               dmxLocal->name);
        return NULL;
    }
    pDevice->public.devicePrivate = dmxLocal;
    dmxLocal->pDevice             = pDevice;

    devname       = dmxMakeUniqueDeviceName(dmxLocal);
    atom          = MakeAtom((char *)devname, strlen(devname), TRUE);
    pDevice->type = atom;
    pDevice->name = devname;

    RegisterOtherDevice (pDevice);

    if (dmxLocal->create_private)
        dmxLocal->private = dmxLocal->create_private(pDevice);

    dmxLogInput(dmxInput, "Added %s as %s device called %s%s\n",
                dmxLocal->name, "extension", devname,
                dmxLocal->isCore
                ? " [core]"
                : (dmxLocal->sendsCore
                   ? " [sends core events]"
                   : ""));

    return pDevice;
}

/** Copy the local input information from \a s into a new \a devs slot
 * in \a dmxInput. */
DMXLocalInputInfoPtr dmxInputCopyLocal(DMXInputInfo *dmxInput,
                                       DMXLocalInputInfoPtr s)
{
    DMXLocalInputInfoPtr dmxLocal = xalloc(sizeof(*dmxLocal));
    
    if (!dmxLocal)
        dmxLog(dmxFatal, "DMXLocalInputInfoPtr: out of memory\n");

    memcpy(dmxLocal, s, sizeof(*dmxLocal));
    dmxLocal->inputIdx       = dmxInput->inputIdx;
    dmxLocal->sendsCore      = dmxInput->core;
    dmxLocal->savedSendsCore = dmxInput->core;
    dmxLocal->deviceId       = -1;
    dmxLocal->attached       = -1;

    ++dmxInput->numDevs;
    dmxInput->devs = xrealloc(dmxInput->devs,
                              dmxInput->numDevs * sizeof(*dmxInput->devs));
    dmxInput->devs[dmxInput->numDevs-1] = dmxLocal;
    
    return dmxLocal;
}

int dmxInputExtensionErrorHandler(Display *dsp, char *name, char *reason)
{
    return 0;
}

static XID dmxGetMasterDevice(XDeviceInfo *device)
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

static void dmxInputScanForExtensions(DMXInputInfo *dmxInput, int doXI)
{
    XExtensionVersion    *ext;
    XDeviceInfo          *devices;
    Display              *display;
    int                  num;
    int                  i, j;
    DMXLocalInputInfoPtr dmxLocal;
    int                  (*handler)(Display *, char *, char *);

    display = dmxScreens[dmxInput->scrnIdx].beDisplay;
    
    /* Print out information about the XInput Extension. */
    handler = XSetExtensionErrorHandler(dmxInputExtensionErrorHandler);
    ext     = XQueryInputVersion(display, XI_2_Major, XI_2_Minor);
    XSetExtensionErrorHandler(handler);
    
    if (!ext || ext == (XExtensionVersion *)NoSuchExtension)
    {
        dmxLogInput(dmxInput, "%s is not available\n", INAME);
	return;
    }

    /* Only use XInput Extension if 2.0 or greater */
    if (ext->major_version < 2)
    {
        dmxLogInput(dmxInput, "%s version %d.%d is too old\n",
		    INAME, ext->major_version, ext->minor_version);
	XFree(ext);
	return;
    }

    XQueryExtension (display, INAME, &i, &dmxInput->eventBase, &i);
    
    dmxLogInput(dmxInput, "Locating devices on %s (%s version %d.%d)\n",
		dmxScreens[dmxInput->scrnIdx].name, INAME,
		ext->major_version, ext->minor_version);
    devices = XListInputDevices(display, &num);

    XFree(ext);
    ext = NULL;

    /* Print a list of all devices */
    for (i = 0; i < num; i++) {
	const char *use = "Unknown";
	switch (devices[i].use) {
	case IsXPointer:           use = "XPointer";         break;
	case IsXKeyboard:          use = "XKeyboard";        break;
	case IsXExtensionDevice:   use = "XExtensionDevice"; break;
	case IsXExtensionPointer:  use = "XExtensionPointer"; break;
	case IsXExtensionKeyboard: use = "XExtensionKeyboard"; break;
	}
	dmxLogInput(dmxInput, "  %2d %-10.10s %-16.16s\n",
		    devices[i].id,
		    devices[i].name ? devices[i].name : "",
		    use);
    }

    /* Search for extensions */
    for (i = 0; i < num; i++) {
	switch (devices[i].use) {
	case IsXKeyboard:
	    for (j = 0; j < dmxInput->numDevs; j++) {
		DMXLocalInputInfoPtr dmxL = dmxInput->devs[j];
		if (dmxL->type == DMX_LOCAL_KEYBOARD
		    && dmxL->deviceId < 0) {
		    dmxL->deviceId   = devices[i].id;
		    dmxL->deviceName = (devices[i].name
					? xstrdup(devices[i].name)
					: NULL);
		    dmxL->attached   = dmxGetMasterDevice (&devices[i]);
		    break;
		}
	    }

	    if (j == dmxInput->numDevs)
	    {
		dmxLocal             = dmxInputCopyLocal(dmxInput,
							 &DMXBackendKbd);
		dmxLocal->isCore     = FALSE;
		dmxLocal->sendsCore  = FALSE;
		dmxLocal->deviceId   = devices[i].id;
		dmxLocal->deviceName = (devices[i].name
					? xstrdup(devices[i].name)
					: NULL);
		dmxLocal->attached   = dmxGetMasterDevice (&devices[i]);
	    }
	    break;
	case IsXPointer:
	    for (j = 0; j < dmxInput->numDevs; j++) {
		DMXLocalInputInfoPtr dmxL = dmxInput->devs[j];
		if (dmxL->type == DMX_LOCAL_MOUSE && dmxL->deviceId < 0) {
		    dmxL->deviceId   = devices[i].id;
		    dmxL->deviceName = (devices[i].name
					? xstrdup(devices[i].name)
					: NULL);
		    dmxL->attached   = dmxGetMasterDevice (&devices[i]);
		    break;
		}
	    }

	    if (j == dmxInput->numDevs)
	    {
		dmxLocal             = dmxInputCopyLocal(dmxInput,
							 &DMXBackendMou);
		dmxLocal->isCore     = FALSE;
		dmxLocal->sendsCore  = FALSE;
		dmxLocal->deviceId   = devices[i].id;
		dmxLocal->deviceName = (devices[i].name
					? xstrdup(devices[i].name)
					: NULL);
		dmxLocal->attached   = dmxGetMasterDevice (&devices[i]);
	    }
	    break;
	}
    }
    XFreeDeviceList(devices);
}

/** Re-initialize all the devices described in \a dmxInput.  Called from
    #dmxReconfig before the cursor is redisplayed. */ 
void dmxInputReInit(DMXInputInfo *dmxInput)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++) {
        DMXLocalInputInfoPtr dmxLocal = dmxInput->devs[i];
        if (dmxLocal->reinit)
            dmxLocal->reinit(&dmxLocal->pDevice->public);
    }
}

/** Re-initialize all the devices described in \a dmxInput.  Called from
    #dmxReconfig after the cursor is redisplayed. */ 
void dmxInputLateReInit(DMXInputInfo *dmxInput)
{
    int i;

    for (i = 0; i < dmxInput->numDevs; i++) {
        DMXLocalInputInfoPtr dmxLocal = dmxInput->devs[i];
        if (dmxLocal->latereinit)
            dmxLocal->latereinit(&dmxLocal->pDevice->public);
    }
}

/** Initialize all of the devices described in \a dmxInput. */
void dmxInputInit(DMXInputInfo *dmxInput)
{
    int i;
    int doXI = 1; /* Include by default */
    int found;

    dmxInput->k = dmxInput->m = dmxInput->o = 0;

    for (found = 0, i = 0; i < dmxNumScreens; i++) {
	if (dmxInput->scrnIdx == i) {
	    dmxInputCopyLocal(dmxInput, &DMXBackendMou);
	    dmxInputCopyLocal(dmxInput, &DMXBackendKbd);
	    dmxLogInput(dmxInput,
			"Using backend input from %s at %d\n",
			dmxScreens[i].name, i);
	    ++found;
	    break;
	}
    }

    /* Locate extensions we may be interested in */
    dmxInputScanForExtensions(dmxInput, doXI);
    
    for (i = 0; i < dmxInput->numDevs; i++) {
        DMXLocalInputInfoPtr dmxLocal = dmxInput->devs[i];
        dmxLocal->pDevice = dmxAddDevice(dmxLocal);
    }

    dmxInput->screenEventCheck      = dmxScreenEventCheck;
    dmxInput->screenReplyCheck      = dmxScreenReplyCheck;
    dmxInput->processInputEvents    = dmxProcessInputEvents;
    dmxInput->grabButton            = dmxGrabButton;
    dmxInput->ungrabButton          = dmxUngrabButton;
    dmxInput->grabPointer           = dmxGrabPointer;
    dmxInput->ungrabPointer         = dmxUngrabPointer;
    dmxInput->detached              = False;
}

static void dmxInputFreeLocal(DMXLocalInputInfoRec *local)
{
    if (!local) return;
    if (local->isCore && local->type == DMX_LOCAL_MOUSE)
        dmxLocalCorePointer  = NULL;
    if (local->isCore && local->type == DMX_LOCAL_KEYBOARD)
        dmxLocalCoreKeyboard = NULL;
    if (local->destroy_private) local->destroy_private(local->private);
    if (local->history)         xfree(local->history);
    if (local->valuators)       xfree(local->valuators);
    if (local->deviceName)      xfree(local->deviceName);
    local->private    = NULL;
    local->history    = NULL;
    local->deviceName = NULL;
    xfree(local);
}

static void dmxInputFini(DMXInputInfo *dmxInput)
{
    int i;
    
    if (dmxInput->keycodes) xfree(dmxInput->keycodes);
    if (dmxInput->symbols)  xfree(dmxInput->symbols);
    if (dmxInput->geometry) xfree(dmxInput->geometry);

    for (i = 0; i < dmxInput->numDevs; i++) {
        dmxInputFreeLocal(dmxInput->devs[i]);
        dmxInput->devs[i] = NULL;
    }
    xfree(dmxInput->devs);
    dmxInput->devs    = NULL;
    dmxInput->numDevs = 0;

    dmxInput->k = dmxInput->m = dmxInput->o = 0;
}

/** Free all of the memory associated with \a dmxInput */
void dmxInputFree(DMXInputInfo *dmxInput)
{
    if (!dmxInput) return;

    dmxInputFini (dmxInput);
}

/** Log information about all of the known devices using #dmxLog(). */
void dmxInputLogDevices(void)
{
    int i, j;

    dmxLog(dmxInfo, "%d devices:\n", dmxGetInputCount());
    dmxLog(dmxInfo, "  Id  Name                 Classes\n");
    for (j = 0; j < dmxNumInputs; j++) {
        DMXInputInfo *dmxInput = &dmxInputs[j];

	for (i = 0; i < dmxInput->numDevs; i++) {
            DeviceIntPtr pDevice = dmxInput->devs[i]->pDevice;
            if (pDevice) {
                dmxLog(dmxInfo, "  %2d%c %-20.20s",
                       pDevice->id,
                       dmxInput->detached ? 'D' : ' ',
                       pDevice->name);
                if (pDevice->key)        dmxLogCont(dmxInfo, " key");
                if (pDevice->valuator)   dmxLogCont(dmxInfo, " val");
                if (pDevice->button)     dmxLogCont(dmxInfo, " btn");
                if (pDevice->focus)      dmxLogCont(dmxInfo, " foc");
                if (pDevice->kbdfeed)    dmxLogCont(dmxInfo, " fb/kbd");
                if (pDevice->ptrfeed)    dmxLogCont(dmxInfo, " fb/ptr");
                if (pDevice->intfeed)    dmxLogCont(dmxInfo, " fb/int");
                if (pDevice->stringfeed) dmxLogCont(dmxInfo, " fb/str");
                if (pDevice->bell)       dmxLogCont(dmxInfo, " fb/bel");
                if (pDevice->leds)       dmxLogCont(dmxInfo, " fb/led");
                if (!pDevice->key && !pDevice->valuator && !pDevice->button
                    && !pDevice->focus && !pDevice->kbdfeed
                    && !pDevice->ptrfeed && !pDevice->intfeed
                    && !pDevice->stringfeed && !pDevice->bell
                    && !pDevice->leds)   dmxLogCont(dmxInfo, " (none)");
                                                                 
                dmxLogCont(dmxInfo, "\t[i%d/%s",
                           dmxInput->inputIdx,
			   dmxScreens[dmxInput->scrnIdx].name);
                if (dmxInput->devs[i]->deviceId >= 0)
                    dmxLogCont(dmxInfo, "/id%d", dmxInput->devs[i]->deviceId);
		if (dmxInput->devs[i]->attached >= 0)
                    dmxLogCont(dmxInfo, "/a%d", dmxInput->devs[i]->attached);
                if (dmxInput->devs[i]->deviceName)
                    dmxLogCont(dmxInfo, "=%s", dmxInput->devs[i]->deviceName);
                dmxLogCont(dmxInfo, "] %s\n",
                           dmxInput->devs[i]->isCore
                           ? "core"
                           : (dmxInput->devs[i]->sendsCore
                              ? "extension (sends core events)"
                              : "extension"));
            }
        }
    }
}

/** Detach an input */
int dmxInputDetach(DMXInputInfo *dmxInput)
{
    int i;

    if (dmxInput->detached) return BadAccess;

    for (i = 0; i < dmxInput->numDevs; i++) {
        DMXLocalInputInfoPtr dmxLocal = dmxInput->devs[i];
        dmxLogInput(dmxInput, "Detaching device id %d: %s%s\n",
                    dmxLocal->pDevice->id,
                    dmxLocal->pDevice->name,
                    dmxLocal->isCore
                    ? " [core]"
                    : (dmxLocal->sendsCore
                       ? " [sends core events]"
                       : ""));
        DisableDevice (dmxLocal->pDevice);
	ProcessInputEvents ();
	DeleteInputDeviceRequest (dmxLocal->pDevice);
    }

    dmxInputFini (dmxInput);
    dmxInput->detached = True;
    dmxInputLogDevices();
    return 0;
}

/** Search for input associated with \a dmxScreen, and detach. */
void dmxInputDetachAll(DMXScreenInfo *dmxScreen)
{
    int i;

    for (i = 0; i < dmxNumInputs; i++) {
        DMXInputInfo *dmxInput = &dmxInputs[i];

        dmxLogInput(dmxInput, "Detaching input: %d - %d -- %d %d\n",
		    i, dmxInput->numDevs, dmxInput->scrnIdx,
		    dmxInput->detached);

        if (dmxInput->scrnIdx == dmxScreen->index) dmxInputDetach(dmxInput);
    }
}

/** Search for input associated with \a deviceId, and detach. */
int dmxInputDetachId(int id)
{
    DMXInputInfo *dmxInput = dmxInputLocateId(id);

    if (!dmxInput) return BadValue;
    
    return dmxInputDetach(dmxInput);
}

DMXInputInfo *dmxInputLocateId(int id)
{
    int i, j;
    
    for (i = 0; i < dmxNumInputs; i++) {
        DMXInputInfo *dmxInput = &dmxInputs[i];
        for (j = 0; j < dmxInput->numDevs; j++) {
            DMXLocalInputInfoPtr dmxLocal = dmxInput->devs[j];
            if (dmxLocal->pDevice->id == id) return dmxInput;
        }
    }
    return NULL;
}

static int dmxInputAttach(DMXInputInfo *dmxInput, int *id)
{
    int i;
    
    dmxInput->detached = False;

    dmxInputInit(dmxInput);

    for (i = 0; i < dmxInput->numDevs; i++) {
        DMXLocalInputInfoPtr dmxLocal = dmxInput->devs[i];
	if (ActivateDevice(dmxLocal->pDevice) != Success ||
            EnableDevice(dmxLocal->pDevice) != TRUE) {
            ErrorF ("[dmx] couldn't add or enable device\n");
            return BadImplementation;
        }
    }
    if (id && dmxInput->devs) *id = dmxInput->devs[0]->pDevice->id;
    dmxInputLogDevices();
    return 0;
}

int dmxInputAttachConsole(const char *name, int isCore, int *id)
{
    return BadImplementation;
}

int dmxInputAttachBackend(int physicalScreen, int isCore, int *id)
{
    DMXInputInfo  *dmxInput;
    DMXScreenInfo *dmxScreen;
    int           i;
    
    if (physicalScreen < 0 || physicalScreen >= dmxNumScreens) return BadValue;
    for (i = 0; i < dmxNumInputs; i++) {
        dmxInput = &dmxInputs[i];
        if (dmxInput->scrnIdx == physicalScreen) {
	    /* Found match */
            if (!dmxInput->detached) return BadAccess; /* Already attached */
            dmxScreen = &dmxScreens[physicalScreen];
            if (!dmxScreen->beDisplay) return BadAccess; /* Screen detached */
            dmxLogInput(dmxInput, "Reattaching detached backend input\n");
            return dmxInputAttach(dmxInput, id);
        }
    }
    return BadImplementation;
}
