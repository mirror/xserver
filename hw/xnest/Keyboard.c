/* $Xorg: Keyboard.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/* $XdotOrg: xserver/xorg/hw/xnest/Keyboard.c,v 1.10 2006-05-29 11:14:03 daniels Exp $ */
/*

   Copyright 1993 by Davor Matic

   Permission to use, copy, modify, distribute, and sell this software
   and its documentation for any purpose is hereby granted without fee,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation.  Davor Matic makes no representations about
   the suitability of this software for any purpose.  It is provided "as
   is" without express or implied warranty.

*/
/* $XFree86: xc/programs/Xserver/hw/xnest/Keyboard.c,v 1.9 2003/09/13 21:33:09 dawes Exp $ */

#define NEED_EVENTS
#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_keysyms.h>
#include "screenint.h"
#include "inputstr.h"
#include "misc.h"
#include "scrnintstr.h"
#include "servermd.h"

#include "Xnest.h"

#include "Display.h"
#include "Screen.h"
#include "Keyboard.h"
#include "Args.h"
#include "Events.h"

#ifdef XKB
#include <X11/extensions/XKB.h>
#include <X11/extensions/XKBsrv.h>
#include <X11/extensions/XKBconfig.h>

extern Bool
XkbQueryExtension(
        Display *		/* dpy */,
        int *			/* opcodeReturn */,
        int *			/* eventBaseReturn */,
        int *			/* errorBaseReturn */,
        int *			/* majorRtrn */,
        int *			/* minorRtrn */
        );

extern	XkbDescPtr XkbGetKeyboard(
        Display *		/* dpy */,
        unsigned int		/* which */,
        unsigned int		/* deviceSpec */
        );

extern	Status	XkbGetControls(
        Display *		/* dpy */,
        unsigned long		/* which */,
        XkbDescPtr		/* desc */
        );

#ifndef XKB_BASE_DIRECTORY
#define	XKB_BASE_DIRECTORY	"/usr/X11R6/lib/X11/xkb/"
#endif
#ifndef XKB_CONFIG_FILE
#define	XKB_CONFIG_FILE		"X0-config.keyboard"
#endif
#ifndef XKB_DFLT_RULES_FILE
#define	XKB_DFLT_RULES_FILE	__XKBDEFRULES__
#endif
#ifndef XKB_DFLT_KB_LAYOUT
#define	XKB_DFLT_KB_LAYOUT	"us"
#endif
#ifndef XKB_DFLT_KB_MODEL
#define	XKB_DFLT_KB_MODEL	"pc101"
#endif
#ifndef XKB_DFLT_KB_VARIANT
#define	XKB_DFLT_KB_VARIANT	NULL
#endif
#ifndef XKB_DFLT_KB_OPTIONS
#define	XKB_DFLT_KB_OPTIONS	NULL
#endif

#endif

DeviceIntPtr xnestKeyboardDevice = NULL;

void xnestBell(int volume, DeviceIntPtr pDev, pointer ctrl, int cls)
{
    XCBBell(xnestConnection, volume);
}

void xnestChangeKeyboardControl(DeviceIntPtr pDev, KeybdCtrl *ctrl)
{
#if 0
    unsigned long value_mask;
    XKeyboardControl values;
    int i;

    value_mask = KBKeyClickPercent |
        KBBellPercent |
        KBBellPitch |
        KBBellDuration |
        KBAutoRepeatMode;

    values.key_click_percent = ctrl->click;
    values.bell_percent = ctrl->bell;
    values.bell_pitch = ctrl->bell_pitch;
    values.bell_duration = ctrl->bell_duration;
    values.auto_repeat_mode = ctrl->autoRepeat ? 
        AutoRepeatModeOn : AutoRepeatModeOff;

    XChangeKeyboardControl(xnestDisplay, value_mask, &values);

    /*
       value_mask = KBKey | KBAutoRepeatMode;
       At this point, we need to walk through the vector and compare it
       to the current server vector.  If there are differences, report them.
       */

    value_mask = KBLed | KBLedMode;
    for (i = 1; i <= 32; i++) {
        values.led = i;
        values.led_mode = (ctrl->leds & (1 << (i - 1))) ? LedModeOn : LedModeOff;
        XChangeKeyboardControl(xnestDisplay, value_mask, &values);
    }
#endif
}

int xnestKeyboardProc(DeviceIntPtr pDev, int onoff)
{
    const XCBSetup *setup;
    XCBGetModifierMappingCookie modmapcook;
    XCBGetModifierMappingRep   *modmaprep;
    XCBGetKeyboardMappingCookie keymapcook;
    XCBGetKeyboardMappingRep   *keymaprep;
    XCBGetKeyboardControlCookie ctlcook;
    XCBGetKeyboardControlRep   *ctlvals;
    XCBKEYCODE *modifier_keymap;
    XCBKEYSYM *keymap;  
    int keycodes_per_mod;
    int mapWidth;
    XCBKEYCODE min_keycode, max_keycode;
    KeySymsRec keySyms;
    CARD8 modmap[MAP_LENGTH];
    int i, j;

    switch (onoff)
    {
        case DEVICE_INIT: 
            modmapcook = XCBGetModifierMapping(xnestConnection);
            modmaprep = XCBGetModifierMappingReply(xnestConnection, modmapcook, NULL);
            modifier_keymap = XCBGetModifierMappingKeycodes(modmaprep);
            keycodes_per_mod = modmaprep->keycodes_per_modifier;
            setup = XCBGetSetup(xnestConnection);
            min_keycode = setup->min_keycode;
            max_keycode = setup->max_keycode;
#if 0
//#ifdef _XSERVER64
            {
                KeySym64 *keymap64;
                int i, len;
                keymap64 = XGetKeyboardMapping(xnestDisplay,
                        min_keycode,
                        max_keycode - min_keycode + 1,
                        &mapWidth);
                len = (max_keycode - min_keycode + 1) * mapWidth;
                keymap = (KeySym *)xalloc(len * sizeof(KeySym));
                for(i = 0; i < len; ++i)
                    keymap[i] = keymap64[i];
                XFree(keymap64);
            }
//#else
#endif
            keymapcook = XCBGetKeyboardMapping(xnestConnection, 
                    min_keycode,
                    min_keycode.id-max_keycode.id+1);
            keymaprep = XCBGetKeyboardMappingReply(xnestConnection, keymapcook, NULL);
            keymap = XCBGetKeyboardMappingKeysyms(keymaprep);
            mapWidth = keymaprep->length;
            //XFree(keymap); Do we leak? X doesn't like freeing this...
            /*#endif*/

            for (i = 0; i < MAP_LENGTH; i++)
                modmap[i] = 0;
            for (j = 0; j < 8; j++)
                for(i = 0; i < keycodes_per_mod; i++) {
                    CARD8 keycode;
                    if ((keycode = modifier_keymap[j * keycodes_per_mod + i].id))
                        modmap[keycode] |= 1<<j;
                }
            //XFree(modifier_keymap); Freeing this also doesn't want to work..

            keySyms.minKeyCode = min_keycode.id;
            keySyms.maxKeyCode = max_keycode.id;
            keySyms.mapWidth = mapWidth;
            keySyms.map = (KeySym *)keymap;
/*
#ifdef XKB
            if (noXkbExtension) {
XkbError:
#endif*/
                ctlcook = XCBGetKeyboardControl(xnestConnection);
                ctlvals = XCBGetKeyboardControlReply(xnestConnection, ctlcook, NULL);
                memmove(defaultKeyboardControl.autoRepeats, 
                        ctlvals->auto_repeats, 
                        sizeof(ctlvals->auto_repeats));
                InitKeyboardDeviceStruct(&pDev->public, 
                                         &keySyms, 
                                         modmap, 
                                         xnestBell, 
                                         xnestChangeKeyboardControl);
/*
#ifdef XKB
            } else {
                FILE *file;
                XkbConfigRtrnRec config;

                XkbComponentNamesRec names;
                char *rules, *model, *layout, *variants, *options;

                XkbDescPtr xkb;
                int op, event, error, major, minor;

                if (XkbQueryExtension(xnestConnection, &op, &event, &error, &major, &minor) == 0) {
                    ErrorF("Unable to initialize XKEYBOARD extension.\n");
                    goto XkbError;
                }
                xkb = XkbGetKeyboard(xnestDisplay, XkbGBN_AllComponentsMask, XkbUseCoreKbd);
                if (xkb == NULL || xkb->geom == NULL) {
                    ErrorF("Couldn't get keyboard.\n");
                    goto XkbError;
                }
                XkbGetControls(xnestDisplay, XkbAllControlsMask, xkb);

                memset(&names, 0, sizeof(XkbComponentNamesRec));
                rules = XKB_DFLT_RULES_FILE;
                model = XKB_DFLT_KB_MODEL;
                layout = XKB_DFLT_KB_LAYOUT;
                variants = XKB_DFLT_KB_VARIANT;
                options = XKB_DFLT_KB_OPTIONS;

                XkbSetRulesDflts(rules, model, layout, variants, options);
                XkbInitKeyboardDeviceStruct(pDev, &names, &keySyms, modmap,
                        xnestBell, xnestChangeKeyboardControl);
                XkbDDXChangeControls(pDev, xkb->ctrls, xkb->ctrls);
                XkbFreeKeyboard(xkb, 0, False);
            }
#endif
#ifdef _XSERVER64
              xfree(keymap);
#else*/
            //XFree(keymap); or this...
/*#endif*/
            break;
        case DEVICE_ON:
            xnestEventMask |= XNEST_KEYBOARD_EVENT_MASK;
            for (i = 0; i < xnestNumScreens; i++)
                XCBChangeWindowAttributes(xnestConnection, xnestDefaultWindows[i], 
                        XCBCWEventMask, &xnestEventMask);
            break;
        case DEVICE_OFF: 
            xnestEventMask &= ~XNEST_KEYBOARD_EVENT_MASK;
            for (i = 0; i < xnestNumScreens; i++)
                XCBChangeWindowAttributes(xnestConnection, xnestDefaultWindows[i], 
                        XCBCWEventMask, &xnestEventMask);
            break;
        case DEVICE_CLOSE: 
            break;
    }
    return Success;
}

Bool LegalModifier(unsigned int key, DevicePtr pDev)
{
    return TRUE;
}

    void
xnestUpdateModifierState(unsigned int state)
{
    DeviceIntPtr pDev = xnestKeyboardDevice;
    KeyClassPtr keyc = pDev->key;
    int i;
    CARD8 mask;

    state = state & 0xff;

    if (keyc->state == state)
        return;

    for (i = 0, mask = 1; i < 8; i++, mask <<= 1) {
        int key;

        /* Modifier is down, but shouldn't be
        */
        if ((keyc->state & mask) && !(state & mask)) {
            int count = keyc->modifierKeyCount[i];

            for (key = 0; key < MAP_LENGTH; key++){
                if (keyc->modifierMap[key] & mask) {
                    int bit;
                    BYTE *kptr;

                    kptr = &keyc->down[key >> 3];
                    bit = 1 << (key & 7);

                    if (*kptr & bit)
                        xnestQueueKeyEvent(KeyRelease, key);

                    if (--count == 0)
                        break;
                }
        }

        /* Modifier shoud be down, but isn't
        */
        if (!(keyc->state & mask) && (state & mask))
            for (key = 0; key < MAP_LENGTH; key++)
                if (keyc->modifierMap[key] & mask) {
                    xnestQueueKeyEvent(KeyPress, key);
                    break;
                }
        }
    }
}
