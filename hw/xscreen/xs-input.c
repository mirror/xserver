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
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/shape.h>

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

static void xsChangePointerControl(DeviceIntPtr pDev UNUSED, PtrCtrl *ctl)
{
    xcb_change_pointer_control(xsConnection, 
                            ctl->num, ctl->den,
                            ctl->threshold,
                            TRUE, TRUE);
}
/**
 * Manages initializing and setting up the pointer.
 **/
static int xsPtrProc(DeviceIntPtr pDev, int state)
{
    uint8_t map[MAX_BUTTONS];
    xcb_get_pointer_mapping_cookie_t c;
    xcb_get_pointer_mapping_reply_t *r;
    int nmap;
    int i;

    switch (state)
    {
        case DEVICE_INIT: 
            c = xcb_get_pointer_mapping(xsConnection);
            r = xcb_get_pointer_mapping_reply(xsConnection, c, NULL);
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
static void xsBell(int vol UNUSED, DeviceIntPtr pDev UNUSED, pointer ctl UNUSED, int wtf_is_this UNUSED)
{
    return;
}

/*no-op function*/
static void xsKbdCtl(DeviceIntPtr pDev UNUSED, KeybdCtrl *ctl UNUSED)
{
}


/**
 * Manages initializing and setting up the keyboard.
 **/
static int xsKbdProc(DeviceIntPtr pDev, int state)
{   
    const xcb_setup_t              *setup;
    xcb_get_keyboard_mapping_cookie_t  mapcook;
    xcb_get_keyboard_mapping_reply_t    *maprep;
    xcb_get_modifier_mapping_cookie_t  modcook;
    xcb_get_modifier_mapping_reply_t    *modrep;
    xcb_get_keyboard_control_cookie_t  ctlcook;
    xcb_get_keyboard_control_reply_t    *ctlrep;


    xcb_keycode_t  min;
    xcb_keycode_t  max;
    xcb_keysym_t  *keysyms;
    xcb_keycode_t *modcodes;

    KeySymsRec  keys;
    uint8_t       modmap[MAP_LENGTH] = {0};
    uint8_t       keycode;
    int         i;
    int         j;

    setup = xcb_get_setup(xsConnection);
    switch (state) 
    {
        case DEVICE_INIT:
            min = setup->min_keycode;
            max = setup->max_keycode;

            /*do all the requests*/
            mapcook = xcb_get_keyboard_mapping(xsConnection, min, max - min);
            modcook = xcb_get_modifier_mapping(xsConnection);
            ctlcook = xcb_get_keyboard_control(xsConnection);

            /*wait for the keyboard mapping*/
            maprep = xcb_get_keyboard_mapping_reply(xsConnection, mapcook, NULL);
            keysyms = xcb_get_keyboard_mapping_keysyms(maprep);

            /* initialize the keycode list*/
            keys.minKeyCode = min;
            keys.maxKeyCode = max;
            keys.mapWidth = maprep->keysyms_per_keycode;
            keys.map = (KeySym *)keysyms;
            
            /*wait for the modifier mapping*/
            modrep = xcb_get_modifier_mapping_reply(xsConnection, modcook, NULL);
            modcodes = xcb_get_modifier_mapping_keycodes(modrep);


            /*initialize the modifiers*/
            for (j = 0; j < 8; j++) {
                for (i = 0; i < modrep->keycodes_per_modifier; i++) {
                    keycode = modcodes[j * modrep->keycodes_per_modifier + i];
                    if (keycode != 0)
                        modmap[keycode] |= 1<<j;
                }
            }

            /*wait for the ctl values*/
            ctlrep = xcb_get_keyboard_control_reply(xsConnection, ctlcook, NULL);
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

static void xsBlockHandler(pointer blockData, OSTimePtr pTimeout, pointer pReadMask)
{
    /*handle events here*/
    xcb_flush(xsConnection);
}

static void xsWakeupHandler(pointer blockData, int result, pointer pReadMask)
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

    AddEnabledDevice(xcb_get_file_descriptor(xsConnection));
    RegisterBlockAndWakeupHandlers(xsBlockHandler, xsWakeupHandler, NULL);
}

