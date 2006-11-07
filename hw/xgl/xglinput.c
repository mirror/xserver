/*
 * Copyright © 2004 David Reveman
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * David Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * David Reveman makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include "xgl.h"
#include "inputstr.h"
#include "mipointer.h"

#define XK_PUBLISHING
#include <X11/keysym.h>
#if HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif

#define NUM_BUTTONS 7

#ifdef XEVDEV
#include <X11/Xevdev.h>

static CARD32 lastEventTime = 0;

static int EventToXserver[] = {
    0,      1,      2,      3,      4,      5,      6,      7,
    8,      9,      10,     11,     12,     13,     14,     15,
    16,     17,     18,     19,     20,     21,     22,     23,
    24,     25,     26,     27,     28,     29,     30,     31,
    32,     33,     34,     35,     36,     37,     38,     39,
    40,     41,     42,     43,     44,     45,     46,     47,
    48,     49,     50,     51,     52,     53,     54,     55,
    56,     57,     58,     59,     60,     61,     62,     63,
    64,     65,     66,     67,     68,     69,     70,     71,
    72,     73,     74,     75,     76,     77,     78,     79,
    80,     81,     82,     83,     84,     85,     86,     87,
    88,     203,    90,     91,     92,     93,     94,     126,
    100,    101,    104,    99,     105,    101,    89,     90,
    91,     92,     94,     95,     96,     97,     98,     99,
    112,    113,    114,    115,    214,    117,    118,    102,
    120,    126,    122,    123,    124,    125,    126,    127,
    128,    129,    130,    131,    132,    133,    134,    135,
    136,    137,    138,    109,    140,    141,    215,    219,
    144,    145,    146,    147,    148,    149,    150,    151,
    152,    153,    154,    155,    156,    157,    158,    159,
    160,    161,    162,    163,    164,    165,    166,    167,
    168,    169,    170,    171,    172,    173,    174,    175,
    176,    177,    178,    179,    180,    181,    182,    183,
    184,    185,    186,    187,    188,    189,    190,    191,
    192,    193,    194,    195,    196,    197,    198,    199,
    200,    201,    202,    203,    204,    205,    206,    207,
    208,    209,    210,    211,    212,    213,    214,    215,
    216,    217,    218,    219,    220,    221,    222,    223,
    224,    225,    226,    227,    228,    229,    230,    231,
    232,    233,    234,    235,    236,    237,    238,    239,
    240,    241,    242,    243,    244,    245,    246,    247,
    248
};

void
xglEvdevReadInput (void)
{
    struct input_event ie;
    xEvent             x;
    int                i;

    while (EvdevCheckIfEvent (&ie))
    {
	switch (ie.type) {
	case EV_REL:
	    switch (ie.code) {
	    case REL_X:
		miPointerDeltaCursor (ie.value, 0,
				      lastEventTime = GetTimeInMillis ());
		break;
	    case REL_Y:
		miPointerDeltaCursor (0, ie.value,
				      lastEventTime = GetTimeInMillis ());
		break;
	    case REL_WHEEL:
		if (ie.value > 0)
		{
		    x.u.u.detail = 4;
		}
		else
		{
		    x.u.u.detail = 5;
		    ie.value *= -1;
		}

		for (i = 0; i < ie.value; i++)
		{
		    x.u.u.type = ButtonPress;
		    mieqEnqueue (&x);

		    x.u.u.type = ButtonRelease;
		    mieqEnqueue (&x);
		}
		break;
	    case REL_HWHEEL:
		if (ie.value > 0)
		{
		    x.u.u.detail = 6;
		}
		else
		{
		    x.u.u.detail = 7;
		    ie.value *= -1;
		}

		for (i = 0; i < ie.value; i++)
		{
		    x.u.u.type = ButtonPress;
		    mieqEnqueue (&x);

		    x.u.u.type = ButtonRelease;
		    mieqEnqueue (&x);
		}
		break;
	    }
	    break;
	case EV_ABS:
	    break;
	case EV_KEY:
	    if (ie.code >= BTN_MOUSE && ie.type < BTN_JOYSTICK)
	    {
		x.u.u.type = ie.value ? ButtonPress : ButtonRelease;
		switch (ie.code) {
		case BTN_LEFT:
		    x.u.u.detail = 1;
		    break;
		case BTN_MIDDLE:
		    x.u.u.detail = 2;
		    break;
		case BTN_RIGHT:
		    x.u.u.detail = 3;
		    break;
		default:
		    x.u.u.detail = 2;
		    break;
		}

		x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis ();
		mieqEnqueue(&x);
		break;
	    }
	    else
	    {
		x.u.u.type = ie.value ? KeyPress : KeyRelease;
		x.u.u.detail = EventToXserver[ie.code] + 8;
		x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis ();
		mieqEnqueue (&x);
	    }
	case EV_SYN:
	    break;
	}
    }
}
#endif

int
xglMouseProc (DeviceIntPtr pDevice,
	      int	   onoff)
{
    BYTE      map[NUM_BUTTONS + 1];
    DevicePtr pDev = (DevicePtr) pDevice;
    int       i;

    switch (onoff) {
    case DEVICE_INIT:
	for (i = 1; i <= NUM_BUTTONS; i++)
	    map[i] = i;

	InitPointerDeviceStruct (pDev,
				 map,
				 NUM_BUTTONS,
				 miPointerGetMotionEvents,
				 (PtrCtrlProcPtr) NoopDDA,
				 miPointerGetMotionBufferSize ());
	break;
    case DEVICE_ON:
	pDev->on = TRUE;
	break;
    case DEVICE_OFF:
    case DEVICE_CLOSE:
	pDev->on = FALSE;
	break;
    }

    return Success;
}

void
xglBell (int	      volume,
	 DeviceIntPtr pDev,
	 pointer      ctrl,
	 int	      something)
{
}

void
xglKbdCtrl (DeviceIntPtr pDevice,
	    KeybdCtrl	 *ctrl)
{
}

#define XGL_KEYMAP_WIDTH    2

KeySym xglKeymap[] = {
/*      1     8 */	 XK_Escape, NoSymbol,
/*      2     9 */	 XK_1,	XK_exclam,
/*      3    10 */	 XK_2,	XK_at,
/*      4    11 */	 XK_3,	XK_numbersign,
/*      5    12 */	 XK_4,	XK_dollar,
/*      6    13 */	 XK_5,	XK_percent,
/*      7    14 */	 XK_6,	XK_asciicircum,
/*      8    15 */	 XK_7,	XK_ampersand,
/*      9    16 */	 XK_8,	XK_asterisk,
/*     10    17 */	 XK_9,	XK_parenleft,
/*     11    18 */	 XK_0,	XK_parenright,
/*     12    19 */	 XK_minus,	XK_underscore,
/*     13    20 */	 XK_equal,	XK_plus,
/*     14    21 */	 XK_BackSpace,	NoSymbol,
/*     15    22 */	 XK_Tab,	NoSymbol,
/*     16    23 */	 XK_Q,	NoSymbol,
/*     17    24 */	 XK_W,	NoSymbol,
/*     18    25 */	 XK_E,	NoSymbol,
/*     19    26 */	 XK_R,	NoSymbol,
/*     20    27 */	 XK_T,	NoSymbol,
/*     21    28 */	 XK_Y,	NoSymbol,
/*     22    29 */	 XK_U,	NoSymbol,
/*     23    30 */	 XK_I,	NoSymbol,
/*     24    31 */	 XK_O,	NoSymbol,
/*     25    32 */	 XK_P,	NoSymbol,
/*     26    33 */	 XK_bracketleft,	XK_braceleft,
/*     27    34 */	 XK_bracketright,	XK_braceright,
/*     28    35 */	 XK_Return,	NoSymbol,
/*     29    36 */	 XK_Control_L,	NoSymbol,
/*     30    37 */	 XK_A,	NoSymbol,
/*     31    38 */	 XK_S,	NoSymbol,
/*     32    39 */	 XK_D,	NoSymbol,
/*     33    40 */	 XK_F,	NoSymbol,
/*     34    41 */	 XK_G,	NoSymbol,
/*     35    42 */	 XK_H,	NoSymbol,
/*     36    43 */	 XK_J,	NoSymbol,
/*     37    44 */	 XK_K,	NoSymbol,
/*     38    45 */	 XK_L,	NoSymbol,
/*     39    46 */	 XK_semicolon,	XK_colon,
/*     40    47 */	 XK_apostrophe,	XK_quotedbl,
/*     41    48 */	 XK_grave,	XK_asciitilde,
/*     42    49 */	 XK_Shift_L,	NoSymbol,
/*     43    50 */	 XK_backslash,	XK_bar,
/*     44    51 */	 XK_Z,	NoSymbol,
/*     45    52 */	 XK_X,	NoSymbol,
/*     46    53 */	 XK_C,	NoSymbol,
/*     47    54 */	 XK_V,	NoSymbol,
/*     48    55 */	 XK_B,	NoSymbol,
/*     49    56 */	 XK_N,	NoSymbol,
/*     50    57 */	 XK_M,	NoSymbol,
/*     51    58 */	 XK_comma,	XK_less,
/*     52    59 */	 XK_period,	XK_greater,
/*     53    60 */	 XK_slash,	XK_question,
/*     54    61 */	 XK_Shift_R,	NoSymbol,
/*     55    62 */	 XK_KP_Multiply,	NoSymbol,
/*     56    63 */	 XK_Alt_L,	XK_Meta_L,
/*     57    64 */	 XK_space,	NoSymbol,
/*     58    65 */	 XK_Caps_Lock,	NoSymbol,
/*     59    66 */	 XK_F1,	NoSymbol,
/*     60    67 */	 XK_F2,	NoSymbol,
/*     61    68 */	 XK_F3,	NoSymbol,
/*     62    69 */	 XK_F4,	NoSymbol,
/*     63    70 */	 XK_F5,	NoSymbol,
/*     64    71 */	 XK_F6,	NoSymbol,
/*     65    72 */	 XK_F7,	NoSymbol,
/*     66    73 */	 XK_F8,	NoSymbol,
/*     67    74 */	 XK_F9,	NoSymbol,
/*     68    75 */	 XK_F10,	NoSymbol,
/*     69    76 */	 XK_Break,	XK_Pause,
/*     70    77 */	 XK_Scroll_Lock,	NoSymbol,
/*     71    78 */	 XK_KP_Home,	XK_KP_7,
/*     72    79 */	 XK_KP_Up,	XK_KP_8,
/*     73    80 */	 XK_KP_Page_Up,	XK_KP_9,
/*     74    81 */	 XK_KP_Subtract,	NoSymbol,
/*     75    82 */	 XK_KP_Left,	XK_KP_4,
/*     76    83 */	 XK_KP_5,	NoSymbol,
/*     77    84 */	 XK_KP_Right,	XK_KP_6,
/*     78    85 */	 XK_KP_Add,	NoSymbol,
/*     79    86 */	 XK_KP_End,	XK_KP_1,
/*     80    87 */	 XK_KP_Down,	XK_KP_2,
/*     81    88 */	 XK_KP_Page_Down,	XK_KP_3,
/*     82    89 */	 XK_KP_Insert,	XK_KP_0,
/*     83    90 */	 XK_KP_Delete,	XK_KP_Decimal,
/*     84    91 */     NoSymbol,	NoSymbol,
/*     85    92 */     NoSymbol,	NoSymbol,
/*     86    93 */     NoSymbol,	NoSymbol,
/*     87    94 */	 XK_F11,	NoSymbol,
/*     88    95 */	 XK_F12,	NoSymbol,
/*     89    96 */	 XK_Control_R,	NoSymbol,
/*     90    97 */	 XK_KP_Enter,	NoSymbol,
/*     91    98 */	 XK_KP_Divide,	NoSymbol,
/*     92    99 */	 XK_Sys_Req,	XK_Print,
/*     93   100 */	 XK_Alt_R,	XK_Meta_R,
/*     94   101 */	 XK_Num_Lock,	NoSymbol,
/*     95   102 */	 XK_Home,	NoSymbol,
/*     96   103 */	 XK_Up,		NoSymbol,
/*     97   104 */	 XK_Page_Up,	NoSymbol,
/*     98   105 */	 XK_Left,	NoSymbol,
/*     99   106 */	 XK_Right,	NoSymbol,
/*    100   107 */	 XK_End,	NoSymbol,
/*    101   108 */	 XK_Down,	NoSymbol,
/*    102   109 */	 XK_Page_Down,	NoSymbol,
/*    103   110 */	 XK_Insert,	NoSymbol,
/*    104   111 */	 XK_Delete,	NoSymbol,
/*    105   112 */	 XK_Super_L,	NoSymbol,
/*    106   113 */	 XK_Super_R,	NoSymbol,
/*    107   114 */	 XK_Menu,	NoSymbol,

/*    108   115 */	 XK_Next,	NoSymbol,   /* right button on side */
/*    109   116 */	 XK_Prior,	NoSymbol,   /* left button on side */
/*    110   117 */	 XK_Up,		NoSymbol,   /* joypad */
/*    111   118 */	 XK_Down,	NoSymbol,
/*    112   119 */	 XK_Left,	NoSymbol,
/*    113   120 */	 XK_Right,	NoSymbol,
/*    114   121 */	 NoSymbol,	NoSymbol,   /* left near speaker */
/*    115   122 */	 NoSymbol,	NoSymbol,   /* right near speaker */
/*    116   123 */	 NoSymbol,	NoSymbol,   /* tiny button */
};

CARD8 xglModMap[MAP_LENGTH];

KeySymsRec xglKeySyms = {
    xglKeymap,
    8,
    8 + (sizeof (xglKeymap) / sizeof (xglKeymap[0]) / XGL_KEYMAP_WIDTH) - 1,
    XGL_KEYMAP_WIDTH
};

int
xglKeybdProc (DeviceIntPtr pDevice,
	      int	   onoff)
{
    Bool      ret;
    DevicePtr pDev = (DevicePtr) pDevice;

    if (!pDev)
	return BadImplementation;

    switch (onoff) {
    case DEVICE_INIT:
	if (pDev != LookupKeyboardDevice ())
	    return !Success;

	ret = InitKeyboardDeviceStruct (pDev,
					&xglKeySyms,
					xglModMap,
					xglBell,
					xglKbdCtrl);

	if (!ret)
	    return BadImplementation;
	break;
    case DEVICE_ON:
	pDev->on = TRUE;
	break;
    case DEVICE_OFF:
    case DEVICE_CLOSE:
	pDev->on = FALSE;
	break;
    }

    return Success;
}

void
xglInitInput (int argc, char **argv)
{
    DeviceIntPtr pKeyboard, pPointer;

    pPointer  = AddInputDevice (xglMouseProc, TRUE);
    pKeyboard = AddInputDevice (xglKeybdProc, TRUE);

    RegisterPointerDevice (pPointer);
    RegisterKeyboardDevice (pKeyboard);

    miRegisterPointerDevice (screenInfo.screens[0], pPointer);
    mieqInit (&pKeyboard->public, &pPointer->public);
}

void
xglWakeupHandler (pointer blockData,
		  int     result,
		  pointer pReadMask)
{
}
