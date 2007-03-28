/*
 * Copyright 2006 Zack Rusin
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * ZACK RUSIN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ZACK RUSIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Alan Hourihane <alanh@tungstengraphics.com>
 *
 * Re-written from original code by Zack Rusin
 * 
 **************************************************************************/

#ifndef GLUCOSE_H
#define GLUCOSE_H

#define GLUCOSE_VERSION_MAJOR   1
#define GLUCOSE_VERSION_MINOR   0
#define GLUCOSE_VERSION_RELEASE 0


#define GLUCOSE_MAKE_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))
#define GLUCOSE_VERSION \
    GLUCOSE_MAKE_VERSION(GLUCOSE_VERSION_MAJOR, GLUCOSE_VERSION_MINOR, GLUCOSE_VERSION_RELEASE)
#define GLUCOSE_IS_VERSION(a,b,c) (GLUCOSE_VERSION >= GLUCOSE_MAKE_VERSION(a,b,c))

Bool
glucoseCloseScreen (int	  index,
		ScreenPtr pScreen);

Bool
glucoseFinishScreenInit (ScreenPtr pScreen);

Bool
glucoseScreenInit (ScreenPtr pScreen);

typedef struct {
    __GLXdrawable *rootDrawable;
    __GLXcontext *rootContext;
    CloseScreenProcPtr CloseScreen;
    __GLXscreen *screen;
} GlucoseScreenPrivRec, *GlucoseScreenPrivPtr;

extern int glucoseScreenPrivateIndex;
#define GlucoseGetScreenPriv(s)	((GlucoseScreenPrivPtr)(s)->devPrivates[glucoseScreenPrivateIndex].ptr)
#define GlucoseScreenPriv(s)	GlucoseScreenPrivPtr    pGlucoseScr = GlucoseGetScreenPriv(s)

#endif /* GLUCOSE_H */
