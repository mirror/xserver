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

#ifndef DMXSHM_H
#define DMXSHM_H

#ifdef MITSHM

#include <X11/extensions/XShm.h>
#include <xcb/shm.h>

typedef struct _dmxShmSegInfo {
    struct _dmxShmSegInfo *next;
    uint32_t              shmid;
    int                   refcnt;
    uint8_t               readOnly;
    uint32_t              pendingEvents;
    xcb_shm_seg_t         shmseg[MAXSCREENS];
    xcb_void_cookie_t     cookie[MAXSCREENS];
} dmxShmSegInfoRec, *dmxShmSegInfoPtr;

extern unsigned long DMX_SHMSEG;

extern void ShmRegisterDmxFuncs (ScreenPtr pScreen);

extern void dmxBEAttachShmSeg (DMXScreenInfo    *dmxScreen,
			       dmxShmSegInfoPtr pShmInfo);
extern void dmxBEDetachShmSeg (DMXScreenInfo    *dmxScreen,
			       dmxShmSegInfoPtr pShmInfo);

extern Bool dmxScreenEventCheckShm (ScreenPtr           pScreen,
				    xcb_generic_event_t *event);

extern Bool dmxShmInit (ScreenPtr pScreen);

extern void dmxInitShm (void);
extern void dmxResetShm (void);
#endif

#endif /* DMXSHM_H */
