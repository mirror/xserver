/**
 * Copyright 2006 Ori Bernstein
 *
 *    Permission to use, copy, modify, distribute, and sell this software
 *    and its documentation for any purpose is hereby granted without fee,
 *    provided that the above copyright notice appear in all copies and that
 *    both that copyright notice and this permission notice appear in
 *    supporting documentation.  Ori Bernstein makes no representations about
 *    the suitability of this software for any purpose.  It is provided "as
 *    is" without express or implied warranty.
 **/

#ifdef HAVE_XSCREEN_CONFIG_H
#include <xs-config.h>
#endif

#include <stdlib.h>

/* need to include Xmd before XCB stuff, or
 * things get redeclared.*/
#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/shape.h>

#include "gcstruct.h"
#include "window.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "region.h"
#include "misc.h"

#include "mi.h"
#include "mipointer.h"

#include "xs-globals.h"

#define UNUSED __attribute__((__unused__))

/**
 * FIXME: these numbers stolen from Xnest. Are they correct?
 **/
#define MAX_BUTTONS 256

static DevicePtr xsPtr;
static DevicePtr xsKbd;

/**
 * DIX hook for processing input events.
 * Just hooks into the mi stuff.
 **/
void ProcessInputEvents()
{
    mieqProcessInputEvents();
    miPointerUpdate();
}

/*The backing server should have already filtered invalid modifiers*/
Bool LegalModifier(unsigned int key UNUSED, DevicePtr pDev UNUSED)
{
    return TRUE;
}

void OsVendorInit()
{
}

void xsChangePointerControl(DeviceIntPtr pDev UNUSED, PtrCtrl *ctl)
{
    XCBChangePointerControl(xsConnection, 
                            ctl->num, ctl->den,
                            ctl->threshold,
                            TRUE, TRUE);
}
/**
 * Manages initializing and setting up the pointer.
 **/
int xsPtrProc(DeviceIntPtr pDev, int state)
{
    CARD8 map[MAX_BUTTONS];
    XCBGetPointerMappingCookie c;
    XCBGetPointerMappingRep *r;
    int nmap;
    int i;

    switch (state)
    {
        case DEVICE_INIT: 
            c = XCBGetPointerMapping(xsConnection);
            r = XCBGetPointerMappingReply(xsConnection, c, NULL);
            nmap = r->map_len;
            for (i = 0; i <= nmap; i++)
                map[i] = i; /* buttons are already mapped */
            InitPointerDeviceStruct(&pDev->public, map, nmap,
                                    miPointerGetMotionEvents,
                                    xsChangePointerControl,
                                    miPointerGetMotionBufferSize());
            break;

        /* device is always on, so ignore DEVICE_ON, DEVICE_OFF*/
        default:
            break;
    }
    return Success;
}

/**
 * Keyboard callback functions
 **/

/* no-op function */
void xsBell(int vol UNUSED, DeviceIntPtr pDev UNUSED, pointer ctl UNUSED, int wtf_is_this UNUSED)
{
    return;
}

/*no-op function*/
void xsKbdCtl(DeviceIntPtr pDev UNUSED, KeybdCtrl *ctl UNUSED)
{
}


/**
 * Manages initializing and setting up the keyboard.
 **/
int xsKbdProc(DeviceIntPtr pDev, int state)
{   
    const XCBSetup              *setup;
    XCBGetKeyboardMappingCookie  mapcook;
    XCBGetKeyboardMappingRep    *maprep;
    XCBGetModifierMappingCookie  modcook;
    XCBGetModifierMappingRep    *modrep;
    XCBGetKeyboardControlCookie  ctlcook;
    XCBGetKeyboardControlRep    *ctlrep;


    XCBKEYCODE  min;
    XCBKEYCODE  max;
    XCBKEYSYM  *keysyms;
    XCBKEYCODE *modcodes;

    KeySymsRec  keys;
    CARD8       modmap[MAP_LENGTH] = {0};
    CARD8       keycode;
    int         i;
    int         j;

    setup = XCBGetSetup(xsConnection);
    switch (state) 
    {
        case DEVICE_INIT:
            min = setup->min_keycode;
            max = setup->max_keycode;

            /*do all the requests*/
            mapcook = XCBGetKeyboardMapping(xsConnection, min, max.id - min.id);
            modcook = XCBGetModifierMapping(xsConnection);
            ctlcook = XCBGetKeyboardControl(xsConnection);

            /*wait for the keyboard mapping*/
            maprep = XCBGetKeyboardMappingReply(xsConnection, mapcook, NULL);
            keysyms = XCBGetKeyboardMappingKeysyms(maprep);

            /* initialize the keycode list*/
            keys.minKeyCode = min.id;
            keys.maxKeyCode = max.id;
            keys.mapWidth = maprep->keysyms_per_keycode;
            keys.map = (KeySym *)keysyms;
            
            /*wait for the modifier mapping*/
            modrep = XCBGetModifierMappingReply(xsConnection, modcook, NULL);
            modcodes = XCBGetModifierMappingKeycodes(modrep);


            /*initialize the modifiers*/
            for (j = 0; j < 8; j++) {
                for (i = 0; i < modrep->keycodes_per_modifier; i++) {
                    keycode = modcodes[j * modrep->keycodes_per_modifier + i].id;
                    if (keycode != 0)
                        modmap[keycode] |= 1<<j;
                }
            }

            /*wait for the ctl values*/
            ctlrep = XCBGetKeyboardControlReply(xsConnection, ctlcook, NULL);
            /*initialize the auto repeats*/
            memmove(defaultKeyboardControl.autoRepeats,
                    ctlrep->auto_repeats,
                    sizeof(ctlrep->auto_repeats));

            InitKeyboardDeviceStruct(&pDev->public,
                                     &keys,
                                     modmap, 
                                     xsBell, 
                                     xsKbdCtl);

            break;
    }
    return Success;
}

/**
 * DIX hooks for initializing input and output.
 * XKB stuff is not supported yet, since it's currently missing in
 * XCB.
 **/

void xsBlockHandler(pointer blockData, OSTimePtr pTimeout, pointer pReadMask)
{
    /*handle events here*/
    XCBFlush(xsConnection);
}

void xsWakeupHandler(pointer blockData, int result, pointer pReadMask)
{
    /*handle events here*/
}

void InitInput(int argc, char *argv[])
{
    xsPtr = (DevicePtr) AddInputDevice(xsPtrProc, TRUE);
    xsKbd = (DevicePtr) AddInputDevice(xsKbdProc, TRUE);

    RegisterPointerDevice((DeviceIntPtr)xsPtr);
    RegisterKeyboardDevice((DeviceIntPtr)xsKbd);

    mieqInit(xsKbd, xsPtr);

    AddEnabledDevice(XCBGetFileDescriptor(xsConnection));
    RegisterBlockAndWakeupHandlers(xsBlockHandler, xsWakeupHandler, NULL);
}

