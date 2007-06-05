#undef VERBOSE

/*****************************************************************

Copyright 2007 Sun Microsystems, Inc.

All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, provided that the above
copyright notice(s) and this permission notice appear in all copies of
the Software and that both the above copyright notice(s) and this
permission notice appear in supporting documentation.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Except as contained in this notice, the name of a copyright holder
shall not be used in advertising or otherwise to promote the sale, use
or other dealings in this Software without prior written authorization
of the copyright holder.

Copyright 1985, 1987, 1998  The Open Group

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

******************************************************************/

#include "inputstr.h"
#define XK_MISCELLANY
#define XK_LATIN1
#define XK_LATIN2
#define XK_LATIN3
#define XK_LATIN4
#define XK_LATIN8
#define XK_LATIN9
#define XK_CYRILLIC
#define XK_GREEK
#define XK_ARMENIAN
#define XK_CAUCASUS
#define XK_VIETNAMESE
#define XK_XKB_KEYS
#include <X11/keysym.h>
#include "windowstr.h"
#include "remwin.h"
#include "protocol.h"
#include "lk201kbd.h"

/*
** Accept the event if its client has been assigned control 
** or if nobody has control.
*/
 
#define ACCEPT_EVENT(pScrPriv, clientId) \
    ((pScrPriv)->controller == clientId || \
     (pScrPriv)->controller == -1)

/************************************************************************
** Begin derived from libX11/KeyBind.c
*/

static void
UCSConvertCase(register unsigned code,
                KeySym *lower,
                KeySym *upper )
{
    /* Case conversion for UCS, as in Unicode Data version 4.0.0 */
    /* NB: Only converts simple one-to-one mappings. */

    /* Tables are used where they take less space than     */
    /* the code to work out the mappings. Zero values mean */
    /* undefined code points.                              */

    static unsigned short const IPAExt_upper_mapping[] = { /* part only */
                            0x0181, 0x0186, 0x0255, 0x0189, 0x018A,
    0x0258, 0x018F, 0x025A, 0x0190, 0x025C, 0x025D, 0x025E, 0x025F,
    0x0193, 0x0261, 0x0262, 0x0194, 0x0264, 0x0265, 0x0266, 0x0267,
    0x0197, 0x0196, 0x026A, 0x026B, 0x026C, 0x026D, 0x026E, 0x019C,
    0x0270, 0x0271, 0x019D, 0x0273, 0x0274, 0x019F, 0x0276, 0x0277,
    0x0278, 0x0279, 0x027A, 0x027B, 0x027C, 0x027D, 0x027E, 0x027F,
    0x01A6, 0x0281, 0x0282, 0x01A9, 0x0284, 0x0285, 0x0286, 0x0287,
    0x01AE, 0x0289, 0x01B1, 0x01B2, 0x028C, 0x028D, 0x028E, 0x028F,
    0x0290, 0x0291, 0x01B7
    };

    static unsigned short const LatinExtB_upper_mapping[] = { /* first part only */
    0x0180, 0x0181, 0x0182, 0x0182, 0x0184, 0x0184, 0x0186, 0x0187,
    0x0187, 0x0189, 0x018A, 0x018B, 0x018B, 0x018D, 0x018E, 0x018F,
    0x0190, 0x0191, 0x0191, 0x0193, 0x0194, 0x01F6, 0x0196, 0x0197,
    0x0198, 0x0198, 0x019A, 0x019B, 0x019C, 0x019D, 0x0220, 0x019F,
    0x01A0, 0x01A0, 0x01A2, 0x01A2, 0x01A4, 0x01A4, 0x01A6, 0x01A7,
    0x01A7, 0x01A9, 0x01AA, 0x01AB, 0x01AC, 0x01AC, 0x01AE, 0x01AF,
    0x01AF, 0x01B1, 0x01B2, 0x01B3, 0x01B3, 0x01B5, 0x01B5, 0x01B7,
    0x01B8, 0x01B8, 0x01BA, 0x01BB, 0x01BC, 0x01BC, 0x01BE, 0x01F7,
    0x01C0, 0x01C1, 0x01C2, 0x01C3, 0x01C4, 0x01C4, 0x01C4, 0x01C7,
    0x01C7, 0x01C7, 0x01CA, 0x01CA, 0x01CA
    };

    static unsigned short const LatinExtB_lower_mapping[] = { /* first part only */
    0x0180, 0x0253, 0x0183, 0x0183, 0x0185, 0x0185, 0x0254, 0x0188,
    0x0188, 0x0256, 0x0257, 0x018C, 0x018C, 0x018D, 0x01DD, 0x0259,
    0x025B, 0x0192, 0x0192, 0x0260, 0x0263, 0x0195, 0x0269, 0x0268,
    0x0199, 0x0199, 0x019A, 0x019B, 0x026F, 0x0272, 0x019E, 0x0275,
    0x01A1, 0x01A1, 0x01A3, 0x01A3, 0x01A5, 0x01A5, 0x0280, 0x01A8,
    0x01A8, 0x0283, 0x01AA, 0x01AB, 0x01AD, 0x01AD, 0x0288, 0x01B0,
    0x01B0, 0x028A, 0x028B, 0x01B4, 0x01B4, 0x01B6, 0x01B6, 0x0292,
    0x01B9, 0x01B9, 0x01BA, 0x01BB, 0x01BD, 0x01BD, 0x01BE, 0x01BF,
    0x01C0, 0x01C1, 0x01C2, 0x01C3, 0x01C6, 0x01C6, 0x01C6, 0x01C9,
    0x01C9, 0x01C9, 0x01CC, 0x01CC, 0x01CC
    };
 
    static unsigned short const Greek_upper_mapping[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0374, 0x0375, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x037A, 0x0000, 0x0000, 0x0000, 0x037E, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 0x0384, 0x0385, 0x0386, 0x0387,
    0x0388, 0x0389, 0x038A, 0x0000, 0x038C, 0x0000, 0x038E, 0x038F, 
    0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397,
    0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F, 
    0x03A0, 0x03A1, 0x0000, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7, 
    0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x0386, 0x0388, 0x0389, 0x038A,
    0x03B0, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 
    0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F, 
    0x03A0, 0x03A1, 0x03A3, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7, 
    0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x038C, 0x038E, 0x038F, 0x0000,
    0x0392, 0x0398, 0x03D2, 0x03D3, 0x03D4, 0x03A6, 0x03A0, 0x03D7, 
    0x03D8, 0x03D8, 0x03DA, 0x03DA, 0x03DC, 0x03DC, 0x03DE, 0x03DE, 
    0x03E0, 0x03E0, 0x03E2, 0x03E2, 0x03E4, 0x03E4, 0x03E6, 0x03E6, 
    0x03E8, 0x03E8, 0x03EA, 0x03EA, 0x03EC, 0x03EC, 0x03EE, 0x03EE,
    0x039A, 0x03A1, 0x03F9, 0x03F3, 0x03F4, 0x0395, 0x03F6, 0x03F7, 
    0x03F7, 0x03F9, 0x03FA, 0x03FA, 0x0000, 0x0000, 0x0000, 0x0000
    };

    static unsigned short const Greek_lower_mapping[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0374, 0x0375, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x037A, 0x0000, 0x0000, 0x0000, 0x037E, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0384, 0x0385, 0x03AC, 0x0387,
    0x03AD, 0x03AE, 0x03AF, 0x0000, 0x03CC, 0x0000, 0x03CD, 0x03CE,
    0x0390, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7,
    0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF, 
    0x03C0, 0x03C1, 0x0000, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7,
    0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,
    0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7,
    0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
    0x03C0, 0x03C1, 0x03C2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7,
    0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, 0x0000,
    0x03D0, 0x03D1, 0x03D2, 0x03D3, 0x03D4, 0x03D5, 0x03D6, 0x03D7,
    0x03D9, 0x03D9, 0x03DB, 0x03DB, 0x03DD, 0x03DD, 0x03DF, 0x03DF,
    0x03E1, 0x03E1, 0x03E3, 0x03E3, 0x03E5, 0x03E5, 0x03E7, 0x03E7,
    0x03E9, 0x03E9, 0x03EB, 0x03EB, 0x03ED, 0x03ED, 0x03EF, 0x03EF,
    0x03F0, 0x03F1, 0x03F2, 0x03F3, 0x03B8, 0x03F5, 0x03F6, 0x03F8,
    0x03F8, 0x03F2, 0x03FB, 0x03FB, 0x0000, 0x0000, 0x0000, 0x0000
    };

    static unsigned short const GreekExt_lower_mapping[] = {
    0x1F00, 0x1F01, 0x1F02, 0x1F03, 0x1F04, 0x1F05, 0x1F06, 0x1F07,
    0x1F00, 0x1F01, 0x1F02, 0x1F03, 0x1F04, 0x1F05, 0x1F06, 0x1F07, 
    0x1F10, 0x1F11, 0x1F12, 0x1F13, 0x1F14, 0x1F15, 0x0000, 0x0000,
    0x1F10, 0x1F11, 0x1F12, 0x1F13, 0x1F14, 0x1F15, 0x0000, 0x0000,
    0x1F20, 0x1F21, 0x1F22, 0x1F23, 0x1F24, 0x1F25, 0x1F26, 0x1F27, 
    0x1F20, 0x1F21, 0x1F22, 0x1F23, 0x1F24, 0x1F25, 0x1F26, 0x1F27, 
    0x1F30, 0x1F31, 0x1F32, 0x1F33, 0x1F34, 0x1F35, 0x1F36, 0x1F37, 
    0x1F30, 0x1F31, 0x1F32, 0x1F33, 0x1F34, 0x1F35, 0x1F36, 0x1F37, 
    0x1F40, 0x1F41, 0x1F42, 0x1F43, 0x1F44, 0x1F45, 0x0000, 0x0000,
    0x1F40, 0x1F41, 0x1F42, 0x1F43, 0x1F44, 0x1F45, 0x0000, 0x0000,
    0x1F50, 0x1F51, 0x1F52, 0x1F53, 0x1F54, 0x1F55, 0x1F56, 0x1F57, 
    0x0000, 0x1F51, 0x0000, 0x1F53, 0x0000, 0x1F55, 0x0000, 0x1F57, 
    0x1F60, 0x1F61, 0x1F62, 0x1F63, 0x1F64, 0x1F65, 0x1F66, 0x1F67, 
    0x1F60, 0x1F61, 0x1F62, 0x1F63, 0x1F64, 0x1F65, 0x1F66, 0x1F67, 
    0x1F70, 0x1F71, 0x1F72, 0x1F73, 0x1F74, 0x1F75, 0x1F76, 0x1F77, 
    0x1F78, 0x1F79, 0x1F7A, 0x1F7B, 0x1F7C, 0x1F7D, 0x0000, 0x0000,
    0x1F80, 0x1F81, 0x1F82, 0x1F83, 0x1F84, 0x1F85, 0x1F86, 0x1F87, 
    0x1F80, 0x1F81, 0x1F82, 0x1F83, 0x1F84, 0x1F85, 0x1F86, 0x1F87, 
    0x1F90, 0x1F91, 0x1F92, 0x1F93, 0x1F94, 0x1F95, 0x1F96, 0x1F97, 
    0x1F90, 0x1F91, 0x1F92, 0x1F93, 0x1F94, 0x1F95, 0x1F96, 0x1F97, 
    0x1FA0, 0x1FA1, 0x1FA2, 0x1FA3, 0x1FA4, 0x1FA5, 0x1FA6, 0x1FA7, 
    0x1FA0, 0x1FA1, 0x1FA2, 0x1FA3, 0x1FA4, 0x1FA5, 0x1FA6, 0x1FA7, 
    0x1FB0, 0x1FB1, 0x1FB2, 0x1FB3, 0x1FB4, 0x0000, 0x1FB6, 0x1FB7, 
    0x1FB0, 0x1FB1, 0x1F70, 0x1F71, 0x1FB3, 0x1FBD, 0x1FBE, 0x1FBF, 
    0x1FC0, 0x1FC1, 0x1FC2, 0x1FC3, 0x1FC4, 0x0000, 0x1FC6, 0x1FC7, 
    0x1F72, 0x1F73, 0x1F74, 0x1F75, 0x1FC3, 0x1FCD, 0x1FCE, 0x1FCF, 
    0x1FD0, 0x1FD1, 0x1FD2, 0x1FD3, 0x0000, 0x0000, 0x1FD6, 0x1FD7, 
    0x1FD0, 0x1FD1, 0x1F76, 0x1F77, 0x0000, 0x1FDD, 0x1FDE, 0x1FDF, 
    0x1FE0, 0x1FE1, 0x1FE2, 0x1FE3, 0x1FE4, 0x1FE5, 0x1FE6, 0x1FE7,
    0x1FE0, 0x1FE1, 0x1F7A, 0x1F7B, 0x1FE5, 0x1FED, 0x1FEE, 0x1FEF, 
    0x0000, 0x0000, 0x1FF2, 0x1FF3, 0x1FF4, 0x0000, 0x1FF6, 0x1FF7, 
    0x1F78, 0x1F79, 0x1F7C, 0x1F7D, 0x1FF3, 0x1FFD, 0x1FFE, 0x0000
    };

    static unsigned short const GreekExt_upper_mapping[] = {
    0x1F08, 0x1F09, 0x1F0A, 0x1F0B, 0x1F0C, 0x1F0D, 0x1F0E, 0x1F0F, 
    0x1F08, 0x1F09, 0x1F0A, 0x1F0B, 0x1F0C, 0x1F0D, 0x1F0E, 0x1F0F, 
    0x1F18, 0x1F19, 0x1F1A, 0x1F1B, 0x1F1C, 0x1F1D, 0x0000, 0x0000,
    0x1F18, 0x1F19, 0x1F1A, 0x1F1B, 0x1F1C, 0x1F1D, 0x0000, 0x0000,
    0x1F28, 0x1F29, 0x1F2A, 0x1F2B, 0x1F2C, 0x1F2D, 0x1F2E, 0x1F2F, 
    0x1F28, 0x1F29, 0x1F2A, 0x1F2B, 0x1F2C, 0x1F2D, 0x1F2E, 0x1F2F, 
    0x1F38, 0x1F39, 0x1F3A, 0x1F3B, 0x1F3C, 0x1F3D, 0x1F3E, 0x1F3F, 
    0x1F38, 0x1F39, 0x1F3A, 0x1F3B, 0x1F3C, 0x1F3D, 0x1F3E, 0x1F3F, 
    0x1F48, 0x1F49, 0x1F4A, 0x1F4B, 0x1F4C, 0x1F4D, 0x0000, 0x0000,
    0x1F48, 0x1F49, 0x1F4A, 0x1F4B, 0x1F4C, 0x1F4D, 0x0000, 0x0000,
    0x1F50, 0x1F59, 0x1F52, 0x1F5B, 0x1F54, 0x1F5D, 0x1F56, 0x1F5F, 
    0x0000, 0x1F59, 0x0000, 0x1F5B, 0x0000, 0x1F5D, 0x0000, 0x1F5F, 
    0x1F68, 0x1F69, 0x1F6A, 0x1F6B, 0x1F6C, 0x1F6D, 0x1F6E, 0x1F6F, 
    0x1F68, 0x1F69, 0x1F6A, 0x1F6B, 0x1F6C, 0x1F6D, 0x1F6E, 0x1F6F, 
    0x1FBA, 0x1FBB, 0x1FC8, 0x1FC9, 0x1FCA, 0x1FCB, 0x1FDA, 0x1FDB, 
    0x1FF8, 0x1FF9, 0x1FEA, 0x1FEB, 0x1FFA, 0x1FFB, 0x0000, 0x0000,
    0x1F88, 0x1F89, 0x1F8A, 0x1F8B, 0x1F8C, 0x1F8D, 0x1F8E, 0x1F8F, 
    0x1F88, 0x1F89, 0x1F8A, 0x1F8B, 0x1F8C, 0x1F8D, 0x1F8E, 0x1F8F, 
    0x1F98, 0x1F99, 0x1F9A, 0x1F9B, 0x1F9C, 0x1F9D, 0x1F9E, 0x1F9F, 
    0x1F98, 0x1F99, 0x1F9A, 0x1F9B, 0x1F9C, 0x1F9D, 0x1F9E, 0x1F9F, 
    0x1FA8, 0x1FA9, 0x1FAA, 0x1FAB, 0x1FAC, 0x1FAD, 0x1FAE, 0x1FAF, 
    0x1FA8, 0x1FA9, 0x1FAA, 0x1FAB, 0x1FAC, 0x1FAD, 0x1FAE, 0x1FAF, 
    0x1FB8, 0x1FB9, 0x1FB2, 0x1FBC, 0x1FB4, 0x0000, 0x1FB6, 0x1FB7, 
    0x1FB8, 0x1FB9, 0x1FBA, 0x1FBB, 0x1FBC, 0x1FBD, 0x0399, 0x1FBF, 
    0x1FC0, 0x1FC1, 0x1FC2, 0x1FCC, 0x1FC4, 0x0000, 0x1FC6, 0x1FC7, 
    0x1FC8, 0x1FC9, 0x1FCA, 0x1FCB, 0x1FCC, 0x1FCD, 0x1FCE, 0x1FCF, 
    0x1FD8, 0x1FD9, 0x1FD2, 0x1FD3, 0x0000, 0x0000, 0x1FD6, 0x1FD7, 
    0x1FD8, 0x1FD9, 0x1FDA, 0x1FDB, 0x0000, 0x1FDD, 0x1FDE, 0x1FDF, 
    0x1FE8, 0x1FE9, 0x1FE2, 0x1FE3, 0x1FE4, 0x1FEC, 0x1FE6, 0x1FE7, 
    0x1FE8, 0x1FE9, 0x1FEA, 0x1FEB, 0x1FEC, 0x1FED, 0x1FEE, 0x1FEF, 
    0x0000, 0x0000, 0x1FF2, 0x1FFC, 0x1FF4, 0x0000, 0x1FF6, 0x1FF7, 
    0x1FF8, 0x1FF9, 0x1FFA, 0x1FFB, 0x1FFC, 0x1FFD, 0x1FFE, 0x0000
    };

    *lower = code;
    *upper = code;

    /* Basic Latin and Latin-1 Supplement, U+0000 to U+00FF */
    if (code <= 0x00ff) {
        if (code >= 0x0041 && code <= 0x005a)             /* A-Z */
            *lower += 0x20;
        else if (code >= 0x0061 && code <= 0x007a)        /* a-z */
            *upper -= 0x20;
        else if ( (code >= 0x00c0 && code <= 0x00d6) ||
	          (code >= 0x00d8 && code <= 0x00de) )
            *lower += 0x20;
        else if ( (code >= 0x00e0 && code <= 0x00f6) ||
	          (code >= 0x00f8 && code <= 0x00fe) )
            *upper -= 0x20;
        else if (code == 0x00ff)      /* y with diaeresis */
            *upper = 0x0178;
        else if (code == 0x00b5)      /* micro sign */
            *upper = 0x039c;
	return;
    }

    /* Latin Extended-A, U+0100 to U+017F */
    if (code >= 0x0100 && code <= 0x017f) {
        if ( (code >= 0x0100 && code <= 0x012f) ||
             (code >= 0x0132 && code <= 0x0137) ||
             (code >= 0x014a && code <= 0x0177) ) {
            *upper = code & ~1;
            *lower = code | 1;
        }
        else if ( (code >= 0x0139 && code <= 0x0148) ||
                  (code >= 0x0179 && code <= 0x017e) ) {
            if (code & 1)
	        *lower += 1;
            else
	        *upper -= 1;
        }
        else if (code == 0x0130)
            *lower = 0x0069;
        else if (code == 0x0131)
            *upper = 0x0049;
        else if (code == 0x0178)
            *lower = 0x00ff;
        else if (code == 0x017f)
            *upper = 0x0053;
        return;
    }

    /* Latin Extended-B, U+0180 to U+024F */
    if (code >= 0x0180 && code <= 0x024f) {
        if (code >= 0x01cd && code <= 0x01dc) {
	    if (code & 1)
	       *lower += 1;
	    else
	       *upper -= 1;
        }
        else if ( (code >= 0x01de && code <= 0x01ef) ||
                  (code >= 0x01f4 && code <= 0x01f5) ||
                  (code >= 0x01f8 && code <= 0x021f) ||
                  (code >= 0x0222 && code <= 0x0233) ) {
            *lower |= 1;
            *upper &= ~1;
        }
        else if (code >= 0x0180 && code <= 0x01cc) {
            *lower = LatinExtB_lower_mapping[code - 0x0180];
            *upper = LatinExtB_upper_mapping[code - 0x0180];
        }
        else if (code == 0x01dd)
            *upper = 0x018e;
        else if (code == 0x01f1 || code == 0x01f2) {
            *lower = 0x01f3;
            *upper = 0x01f1;
        }
        else if (code == 0x01f3)
            *upper = 0x01f1;
        else if (code == 0x01f6)
            *lower = 0x0195;
        else if (code == 0x01f7)
            *lower = 0x01bf;
        else if (code == 0x0220)
            *lower = 0x019e;
        return;
    }

    /* IPA Extensions, U+0250 to U+02AF */
    if (code >= 0x0253 && code <= 0x0292) {
        *upper = IPAExt_upper_mapping[code - 0x0253];
    }

    /* Combining Diacritical Marks, U+0300 to U+036F */
    if (code == 0x0345) {
        *upper = 0x0399;
    }

    /* Greek and Coptic, U+0370 to U+03FF */
    if (code >= 0x0370 && code <= 0x03ff) {
        *lower = Greek_lower_mapping[code - 0x0370];
        *upper = Greek_upper_mapping[code - 0x0370];
        if (*upper == 0)
            *upper = code;
        if (*lower == 0)
            *lower = code;
    }

    /* Cyrillic and Cyrillic Supplementary, U+0400 to U+052F */
    if ( (code >= 0x0400 && code <= 0x04ff) ||
         (code >= 0x0500 && code <= 0x052f) ) {
        if (code >= 0x0400 && code <= 0x040f)
            *lower += 0x50;
        else if (code >= 0x0410 && code <= 0x042f)
            *lower += 0x20;
        else if (code >= 0x0430 && code <= 0x044f)
            *upper -= 0x20;
        else if (code >= 0x0450 && code <= 0x045f)
            *upper -= 0x50;
        else if ( (code >= 0x0460 && code <= 0x0481) ||
                  (code >= 0x048a && code <= 0x04bf) ||
	          (code >= 0x04d0 && code <= 0x04f5) ||
	          (code >= 0x04f8 && code <= 0x04f9) ||
                  (code >= 0x0500 && code <= 0x050f) ) {
            *upper &= ~1;
            *lower |= 1;
        }
        else if (code >= 0x04c1 && code <= 0x04ce) {
	    if (code & 1)
	        *lower += 1;
	    else
	        *upper -= 1;
        }
    }

    /* Armenian, U+0530 to U+058F */
    if (code >= 0x0530 && code <= 0x058f) {
        if (code >= 0x0531 && code <= 0x0556)
            *lower += 0x30;
        else if (code >=0x0561 && code <= 0x0586)
            *upper -= 0x30;
    }

    /* Latin Extended Additional, U+1E00 to U+1EFF */
    if (code >= 0x1e00 && code <= 0x1eff) {
        if ( (code >= 0x1e00 && code <= 0x1e95) ||
             (code >= 0x1ea0 && code <= 0x1ef9) ) {
            *upper &= ~1;
            *lower |= 1;
        }
        else if (code == 0x1e9b)
            *upper = 0x1e60;
    }

    /* Greek Extended, U+1F00 to U+1FFF */
    if (code >= 0x1f00 && code <= 0x1fff) {
        *lower = GreekExt_lower_mapping[code - 0x1f00];
        *upper = GreekExt_upper_mapping[code - 0x1f00];
        if (*upper == 0)
            *upper = code;
        if (*lower == 0)
            *lower = code;
    }

    /* Letterlike Symbols, U+2100 to U+214F */
    if (code >= 0x2100 && code <= 0x214f) {
        switch (code) {
        case 0x2126: *lower = 0x03c9; break;
        case 0x212a: *lower = 0x006b; break;
        case 0x212b: *lower = 0x00e5; break;
        }
    }
    /* Number Forms, U+2150 to U+218F */
    else if (code >= 0x2160 && code <= 0x216f)
        *lower += 0x10;
    else if (code >= 0x2170 && code <= 0x217f)
        *upper -= 0x10;
    /* Enclosed Alphanumerics, U+2460 to U+24FF */
    else if (code >= 0x24b6 && code <= 0x24cf)
        *lower += 0x1a;
    else if (code >= 0x24d0 && code <= 0x24e9)
        *upper -= 0x1a;
    /* Halfwidth and Fullwidth Forms, U+FF00 to U+FFEF */
    else if (code >= 0xff21 && code <= 0xff3a)
        *lower += 0x20;
    else if (code >= 0xff41 && code <= 0xff5a)
        *upper -= 0x20;
    /* Deseret, U+10400 to U+104FF */
    else if (code >= 0x10400 && code <= 0x10427)
        *lower += 0x28;
    else if (code >= 0x10428 && code <= 0x1044f)
        *upper -= 0x28;
}

static void
ConvertCase(KeySym sym, KeySym *lower, KeySym *upper)
{
    /* Latin 1 keysym */
    if (sym < 0x100) {
        UCSConvertCase(sym, lower, upper);
	return;
    }

    /* Unicode keysym */
    if ((sym & 0xff000000) == 0x01000000) {
        UCSConvertCase((sym & 0x00ffffff), lower, upper);
        *upper |= 0x01000000;
        *lower |= 0x01000000;
        return;
    }

    /* Legacy keysym */

    *lower = sym;
    *upper = sym;

    switch(sym >> 8) {
    case 1: /* Latin 2 */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym == XK_Aogonek)
	    *lower = XK_aogonek;
	else if (sym >= XK_Lstroke && sym <= XK_Sacute)
	    *lower += (XK_lstroke - XK_Lstroke);
	else if (sym >= XK_Scaron && sym <= XK_Zacute)
	    *lower += (XK_scaron - XK_Scaron);
	else if (sym >= XK_Zcaron && sym <= XK_Zabovedot)
	    *lower += (XK_zcaron - XK_Zcaron);
	else if (sym == XK_aogonek)
	    *upper = XK_Aogonek;
	else if (sym >= XK_lstroke && sym <= XK_sacute)
	    *upper -= (XK_lstroke - XK_Lstroke);
	else if (sym >= XK_scaron && sym <= XK_zacute)
	    *upper -= (XK_scaron - XK_Scaron);
	else if (sym >= XK_zcaron && sym <= XK_zabovedot)
	    *upper -= (XK_zcaron - XK_Zcaron);
	else if (sym >= XK_Racute && sym <= XK_Tcedilla)
	    *lower += (XK_racute - XK_Racute);
	else if (sym >= XK_racute && sym <= XK_tcedilla)
	    *upper -= (XK_racute - XK_Racute);
	break;
    case 2: /* Latin 3 */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym >= XK_Hstroke && sym <= XK_Hcircumflex)
	    *lower += (XK_hstroke - XK_Hstroke);
	else if (sym >= XK_Gbreve && sym <= XK_Jcircumflex)
	    *lower += (XK_gbreve - XK_Gbreve);
	else if (sym >= XK_hstroke && sym <= XK_hcircumflex)
	    *upper -= (XK_hstroke - XK_Hstroke);
	else if (sym >= XK_gbreve && sym <= XK_jcircumflex)
	    *upper -= (XK_gbreve - XK_Gbreve);
	else if (sym >= XK_Cabovedot && sym <= XK_Scircumflex)
	    *lower += (XK_cabovedot - XK_Cabovedot);
	else if (sym >= XK_cabovedot && sym <= XK_scircumflex)
	    *upper -= (XK_cabovedot - XK_Cabovedot);
	break;
    case 3: /* Latin 4 */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym >= XK_Rcedilla && sym <= XK_Tslash)
	    *lower += (XK_rcedilla - XK_Rcedilla);
	else if (sym >= XK_rcedilla && sym <= XK_tslash)
	    *upper -= (XK_rcedilla - XK_Rcedilla);
	else if (sym == XK_ENG)
	    *lower = XK_eng;
	else if (sym == XK_eng)
	    *upper = XK_ENG;
	else if (sym >= XK_Amacron && sym <= XK_Umacron)
	    *lower += (XK_amacron - XK_Amacron);
	else if (sym >= XK_amacron && sym <= XK_umacron)
	    *upper -= (XK_amacron - XK_Amacron);
	break;
    case 6: /* Cyrillic */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym >= XK_Serbian_DJE && sym <= XK_Serbian_DZE)
	    *lower -= (XK_Serbian_DJE - XK_Serbian_dje);
	else if (sym >= XK_Serbian_dje && sym <= XK_Serbian_dze)
	    *upper += (XK_Serbian_DJE - XK_Serbian_dje);
	else if (sym >= XK_Cyrillic_YU && sym <= XK_Cyrillic_HARDSIGN)
	    *lower -= (XK_Cyrillic_YU - XK_Cyrillic_yu);
	else if (sym >= XK_Cyrillic_yu && sym <= XK_Cyrillic_hardsign)
	    *upper += (XK_Cyrillic_YU - XK_Cyrillic_yu);
        break;
    case 7: /* Greek */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym >= XK_Greek_ALPHAaccent && sym <= XK_Greek_OMEGAaccent)
	    *lower += (XK_Greek_alphaaccent - XK_Greek_ALPHAaccent);
	else if (sym >= XK_Greek_alphaaccent && sym <= XK_Greek_omegaaccent &&
		 sym != XK_Greek_iotaaccentdieresis &&
		 sym != XK_Greek_upsilonaccentdieresis)
	    *upper -= (XK_Greek_alphaaccent - XK_Greek_ALPHAaccent);
	else if (sym >= XK_Greek_ALPHA && sym <= XK_Greek_OMEGA)
	    *lower += (XK_Greek_alpha - XK_Greek_ALPHA);
	else if (sym >= XK_Greek_alpha && sym <= XK_Greek_omega &&
		 sym != XK_Greek_finalsmallsigma)
	    *upper -= (XK_Greek_alpha - XK_Greek_ALPHA);
        break;
    case 0x13: /* Latin 9 */
        if (sym == XK_OE)
            *lower = XK_oe;
        else if (sym == XK_oe)
            *upper = XK_OE;
        else if (sym == XK_Ydiaeresis)
            *lower = XK_ydiaeresis;
        break;
    }
}

static KeySym
KeyCodeToKeySym (KeyCode keycode, int col)
{
    KeySymsPtr pks = &inputInfo.keyboard->key->curKeySyms;
    register int per = pks->mapWidth;
    register KeySym *syms;
    KeySym lsym, usym;

    if ((col < 0) || ((col >= per) && (col > 3)) ||
	((int)keycode < pks->minKeyCode) || ((int)keycode > pks->maxKeyCode))
      return NoSymbol;

    syms = &pks->map[(keycode - pks->minKeyCode) * per];
    if (col < 4) {
	if (col > 1) {
	    while ((per > 2) && (syms[per - 1] == NoSymbol))
		per--;
	    if (per < 3)
		col -= 2;
	}
	if ((per <= (col|1)) || (syms[col|1] == NoSymbol)) {
	    ConvertCase(syms[col&~1], &lsym, &usym);
	    if (!(col & 1))
		return lsym;
	    else if (usym == lsym)
		return NoSymbol;
	    else
		return usym;
	}
    }
    return syms[col];
}

static KeyCode
KeysymToKeycode (KeySym ks)
{
    KeySymsPtr pks = &inputInfo.keyboard->key->curKeySyms;
    register int i, j;

    for (j = 0; j < pks->mapWidth; j++) {
	for (i = pks->minKeyCode; i <= pks->maxKeyCode; i++) {
	    if (KeyCodeToKeySym((KeyCode) i, j) == ks)
		return i;
	}
    }
    return 0;
}

/*
** End derived from libX11/KeyBind.c
************************************************************************/

static void
injectKeyEvent (ScreenPtr pScreen, Bool isPressed, KeySym keySym)
{
    DeviceIntPtr pKeyboard = inputInfo.keyboard;
    xEvent event;

    event.u.u.detail = KeysymToKeycode(keySym);

    if (isPressed) {
	event.u.u.type = KeyPress;
    } else {
	event.u.u.type = KeyRelease;
    }
    event.u.keyButtonPointer.time = GetTimeInMillis();
    event.u.keyButtonPointer.state = 0;

    (*pKeyboard->public.processInputProc)(&event, pKeyboard, 1);
}

static void
injectPointerEvent (ScreenPtr pScreen, int wid, int mask, int x, int y)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    DeviceIntPtr pPointer = inputInfo.pointer;
    unsigned long when = GetTimeInMillis();
    xEvent event;
    int button;

    /* 
    ** TODO: for now, absolutize event here. This isn't actually correct (because it
    ** doesn't take into account window position changing after this point but before
    ** picking, but it will suffice for now.
    */
    { 
	WindowPtr pWin = LookupIDByType(wid, RT_WINDOW);
	if (pWin == NULL) {
	    ErrorF("WARNING: cannot find pointer event window %d\n", wid); 
	    return;
	}
	
	/*
	ErrorF("Pointer event on window %d, winrel coord xy = %d, %d\n", wid, x, y);
	*/

	x += pWin->drawable.x;
	y += pWin->drawable.y;

	/*
	ErrorF("Pointer event on window %d, scrabs coord xy = %d, %d\n", wid, x, y);
	*/
    }    


    event.u.u.type = MotionNotify;
    event.u.keyButtonPointer.rootX = x;
    event.u.keyButtonPointer.rootY = y;
    event.u.keyButtonPointer.time = when;;
    (*inputInfo.pointer->public.processInputProc)(&event, inputInfo.pointer, 1);

    for (button = 0; button < 5; button++) {
	int buttonMask = 1 << button;
	int changeMask = mask ^ pScrPriv->controllerButtonMask;
	if (changeMask & buttonMask) {
	    if (mask & buttonMask) {
		event.u.u.type = ButtonPress;
		event.u.keyButtonPointer.time = when;
		event.u.u.detail = button + 1;
	    } else {
		event.u.u.type = ButtonRelease;
		event.u.keyButtonPointer.time = when;
		event.u.u.detail = button + 1;
	    }
	    (*pPointer->public.processInputProc)(&event, pPointer, 1);
	}
    }

    pScrPriv->controllerButtonMask = mask;
}

/*
** The window manager client is the client which has set
** SubstructureRedirect on the root window. We know this 
** because the Appshare window manager does this.
*/

static ClientPtr
findWindowManagerClient (ScreenPtr pScreen) 
{
    WindowPtr pRootWin = WindowTable[pScreen->myNum];
    OtherClients *pOthClient = wOtherClients(pRootWin);

    while (pOthClient != NULL) {
	if ((pOthClient->mask & SubstructureRedirectMask)) {
	    return clients[CLIENT_ID(pOthClient->resource)];
	}
    }

    FatalError("Remote Window: Cannot find window manager client");
}


static void
moveWindow (WindowPtr pWin, int clientId, short x, short y) 
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    int vlist[2];
    int ret;

    if (pScrPriv->pWmClient == NULL) {
	pScrPriv->pWmClient = findWindowManagerClient(pScreen);
    }

    /* Inform clients that an interactive move is occuring */
    pScrPriv->configuringClient = clientId;

    vlist[0] = x;
    vlist[1] = y;
    ret = ConfigureWindow(pWin, (CWX | CWY), (XID *) vlist, pScrPriv->pWmClient);
    if (ret != Success) {
	ErrorF("Move window failed, error = %d\n", ret);
	return;
    }

    pScrPriv->configuringClient = -1;
}

static void
resizeWindow (WindowPtr pWin, int clientId, int w, int h) 
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    int vlist[2];
    int ret;

    if (pScrPriv->pWmClient == NULL) {
	pScrPriv->pWmClient = findWindowManagerClient(pScreen);
    }

    /* Inform clients that an interactive resize is occuring */
    pScrPriv->configuringClient = clientId;

    vlist[0] = w;
    vlist[1] = h;
    ret = ConfigureWindow(pWin, (CWWidth | CWHeight), (XID *) vlist, pScrPriv->pWmClient);
    if (ret != Success) {
	ErrorF("Resize window failed, error = %d\n", ret);
	return;
    }

    pScrPriv->configuringClient = -1;
}

static void
destroyWindow (WindowPtr pWin) 
{
    if (pWin->parent != NULL) {
	FreeResource(pWin->drawable.id, RT_NONE);
    }
}

/*
** Handle client input messages.
*/

void
rwinHandler (ScreenPtr pScreen)
{
    RemwinScreenPrivPtr pScrPriv = REMWIN_GET_SCRPRIV(pScreen);
    RwcommPtr pComm = pScrPriv->pComm;
    int msgType;
    int n;

    msgType = RWCOMM_NEXT_MESSAGE_TYPE_READ(pComm);
    /*ErrorF("msgType = %d\n", msgType);*/

    switch (msgType) {

    case CLIENT_MESSAGE_TYPE_KEY: {
	char buf[KEY_EVENT_MESSAGE_SIZE];
	Bool isPressed;
	KeySym keySym;
	int clientId;

	if (!RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, buf + 1, 
			     KEY_EVENT_MESSAGE_SIZE -1)) {
	    ErrorF("Error reading next message buffer\n");
	    return;
	}
	
	isPressed = KEY_EVENT_MESSAGE_GET_ISPRESSED(buf);
	keySym = KEY_EVENT_MESSAGE_GET_KEYSYM(buf);
	clientId = KEY_EVENT_MESSAGE_GET_CLIENTID(buf);

	swapl(&keySym, n);
	swapl(&clientId, n);

#ifdef VERBOSE
	ErrorF("KeyEvent: isPressed = %d, keySym = %d, clientId = %d\n", 
	       isPressed, (int)keySym, clientId);
#endif

	if (ACCEPT_EVENT(pScrPriv, clientId)) {
	    injectKeyEvent(pScreen, isPressed, keySym);
	}

	break;
    }

    case CLIENT_MESSAGE_TYPE_POINTER: {
	char buf[POINTER_EVENT_MESSAGE_SIZE];
	int mask;
	short x, y;
	int wid;
	int clientId;

	if (!RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, buf + 1,
			     POINTER_EVENT_MESSAGE_SIZE -1)) {
	    ErrorF("Error reading next message buffer\n");
	    return;
	}

	mask = POINTER_EVENT_MESSAGE_GET_MASK(buf);
	x = POINTER_EVENT_MESSAGE_GET_X(buf);
	y = POINTER_EVENT_MESSAGE_GET_Y(buf);
	wid = POINTER_EVENT_MESSAGE_GET_WID(buf);
	clientId = POINTER_EVENT_MESSAGE_GET_CLIENTID(buf);

	swaps(&x, n);
	swaps(&y, n);
	swapl(&wid, n);
	swapl(&clientId, n);

#ifdef VERBOSE
	ErrorF("PointerEvent: mask = 0x%x, x,y = %d, %d, wid = %d, clientId = %d\n", 
	       mask, x, y, wid, clientId);
#endif

	if (ACCEPT_EVENT(pScrPriv, clientId)) {
	    injectPointerEvent(pScreen, wid, mask, x, y);
	}

	break;
    }

    case CLIENT_MESSAGE_TYPE_TAKE_CONTROL: {
	char buf[TAKE_CONTROL_MESSAGE_SIZE];
	int clientId;
	Bool steal;

	if (!RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, buf + 1,
			     TAKE_CONTROL_MESSAGE_SIZE -1)) {
	    ErrorF("Error reading next message buffer\n");
	    return;
	}

	clientId = TAKE_CONTROL_MESSAGE_GET_CLIENTID(buf);
	steal = TAKE_CONTROL_MESSAGE_GET_STEAL(buf);

	swapl(&clientId, n);

#ifdef VERBOSE
	ErrorF("TakeControl: clientId = %d, steal = %d\n", 
	       clientId, steal);
#endif

	rwTakeControl(pScreen, clientId, steal);

	break;
    }

    case CLIENT_MESSAGE_TYPE_SET_WINDOW_TITLES: {
	char hdrBuf[SET_WINDOW_TITLES_MESSAGE_SIZE];
	char *buf;
	short strLen;

	if (!RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, hdrBuf + 1,
			     SET_WINDOW_TITLES_MESSAGE_SIZE -1)) {
	    ErrorF("Error reading next message buffer\n");
	    return;
	}

	strLen = SET_WINDOW_TITLES_MESSAGE_GET_STRLEN(hdrBuf);

	swaps(&strLen, n);

#ifdef VERBOSE
	ErrorF("SetWindowTitles: strLen = %d\n", strLen);
#endif

	buf = xalloc(strLen);
	if (!RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, buf, strLen)) {
	    ErrorF("Error reading next message buffer\n");
	    return;
	}
	
	if (!rwoutSetWindowTitlesWrite(pScreen, strLen, buf)) {
	    ErrorF("Could not send SetWindowTitles message\n");
	    return;
	}
	
	xfree(buf);

	break;
    }

    case CLIENT_MESSAGE_TYPE_MOVE_WINDOW: {
	char buf[MOVE_WINDOW_MESSAGE_SIZE];
	int clientId;
	int wid;
	int x, y;

	if (!RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, buf + 1,
			     MOVE_WINDOW_MESSAGE_SIZE -1)) {
	    ErrorF("Error reading next message buffer\n");
	    return;
	}

	clientId = MOVE_WINDOW_MESSAGE_GET_CLIENT_ID(buf);
	wid = MOVE_WINDOW_MESSAGE_GET_WID(buf);
	x = MOVE_WINDOW_MESSAGE_GET_X(buf);
	y = MOVE_WINDOW_MESSAGE_GET_Y(buf);

	swapl(&clientId, n);
	swapl(&wid, n);
	swaps(&x, n);
	swaps(&y, n);

#ifdef VERBOSE
	ErrorF("MoveWindow: clientId = %d, wid = %d, xy = %d, %d\n", 
	       clientId, wid, x, y);
#endif
	
	WindowPtr pWin = LookupIDByType(wid, RT_WINDOW);
	if (pWin == NULL) {
	    ErrorF("Could not find window to move, wid = %d\n", wid);
	    return;
	}

	moveWindow(pWin, clientId, x, y);
	break;
    }

    case CLIENT_MESSAGE_TYPE_RESIZE_WINDOW: {
	char buf[RESIZE_WINDOW_MESSAGE_SIZE];
	int clientId;
	int wid;
	int w, h;

	if (!RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, buf + 1,
			     RESIZE_WINDOW_MESSAGE_SIZE -1)) {
	    ErrorF("Error reading next message buffer\n");
	    return;
	}

	clientId = RESIZE_WINDOW_MESSAGE_GET_CLIENT_ID(buf);
	wid = RESIZE_WINDOW_MESSAGE_GET_WID(buf);
	w = RESIZE_WINDOW_MESSAGE_GET_WIDTH(buf);
	h = RESIZE_WINDOW_MESSAGE_GET_HEIGHT(buf);

	swapl(&clientId, n);
	swapl(&wid, n);
	swaps(&w, n);
	swaps(&h, n);

#ifdef VERBOSE
	ErrorF("ResizeWindow: clientId = %d, wid = %d, wh = %d, %d\n", 
	       clientId, wid, w, h);
#endif
	
	WindowPtr pWin = LookupIDByType(wid, RT_WINDOW);
	if (pWin == NULL) {
	    ErrorF("Could not find window to resize, wid = %d\n", wid);
	    return;
	}

	resizeWindow(pWin, clientId, w, h);
	break;
    }

    case CLIENT_MESSAGE_TYPE_DESTROY_WINDOW: {
	char buf[DESTROY_WINDOW_MESSAGE_SIZE];
	int wid;

	if (!RWCOMM_NEXT_MESSAGE_BUFFER_READ(pComm, buf + 1,
		     DESTROY_WINDOW_MESSAGE_SIZE -1)) {
	    ErrorF("Error reading next message buffer\n");
	    return;
	}

	wid = DESTROY_WINDOW_MESSAGE_GET_WID(buf);
	swapl(&wid, n);

#ifdef VERBOSE
	ErrorF("DestroyWindow: wid = %d\n", wid);
#endif
	
	WindowPtr pWin = LookupIDByType(wid, RT_WINDOW);
	if (pWin == NULL) {
	    ErrorF("Could not find window to destroy, wid = %d\n", wid);
	    return;
	}

	destroyWindow(pWin);
	break;
    }

    /* TODO */
    case CLIENT_MESSAGE_TYPE_HELLO: 
	FatalError("rwinHandler: client message hello is not yet implemented\n");
	
    case CLIENT_MESSAGE_TYPE_INVALID:
	// Client has disconnected
	ErrorF("Exitting.\n");
	exit(0);
        break;

    default:
	FatalError("rwinHandler: unknown message type %d\n", msgType);

    }
}

