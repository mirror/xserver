/*
 * Copyright © 2007, 2008 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Soft-
 * ware"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, provided that the above copyright
 * notice(s) and this permission notice appear in all copies of the Soft-
 * ware and that both the above copyright notice(s) and this permission
 * notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
 * ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN
 * THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSE-
 * QUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFOR-
 * MANCE OF THIS SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization of
 * the copyright holder.
 *
 * Authors:
 *   Kristian Høgsberg (krh@redhat.com)
 */

#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include <errno.h>
#include <xf86drm.h>
#include "xf86Module.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "dixstruct.h"
#include "dri2.h"
#include "xf86VGAarbiter.h"

#include "xf86.h"

static int dri2ScreenPrivateKeyIndex;
static DevPrivateKey dri2ScreenPrivateKey = &dri2ScreenPrivateKeyIndex;
static int dri2WindowPrivateKeyIndex;
static DevPrivateKey dri2WindowPrivateKey = &dri2WindowPrivateKeyIndex;
static int dri2PixmapPrivateKeyIndex;
static DevPrivateKey dri2PixmapPrivateKey = &dri2PixmapPrivateKeyIndex;

typedef struct _DRI2Drawable {
    unsigned int	 refCount;
    int			 width;
    int			 height;
    DRI2BufferPtr	*buffers;
    int			 bufferCount;
    unsigned int	 swapsPending;
    unsigned int	 flipsPending;
    ClientPtr		 blockedClient;
    int			 swap_interval;
    CARD64		 swap_count;
    CARD64		 last_swap_target; /* most recently queued swap target */
} DRI2DrawableRec, *DRI2DrawablePtr;

typedef struct _DRI2Screen *DRI2ScreenPtr;
typedef struct _DRI2FrameEvent *DRI2FrameEventPtr;

#define container_of(ptr,type,mem) ((type *)((char *)(ptr) - offsetof(type, \
								      mem)))

struct list {
    struct list *prev, *next;
};

static inline void list_init(struct list *l)
{
    l->prev = l;
    l->next = l;
}

static inline void __list_add(struct list *l, struct list *prev,
			      struct list *next)
{
    prev->next = l;
    l->prev = prev;
    l->next = next;
    next->prev = l;
}

static inline void list_add(struct list *l, struct list *head)
{
    __list_add(l, head, head->next);
}

static inline void list_add_tail(struct list *l, struct list *head)
{
    __list_add(l, head->prev, head);
}

static inline void list_del(struct list *l)
{
    l->prev->next = l->next;
    l->next->prev = l->prev;
    list_init(l);
}

static inline Bool list_is_empty(struct list *l)
{
    return l->next == l;
}

#define list_foreach_safe(cur, tmp, head)			\
    for (cur = (head)->next, tmp = cur->next; cur != (head);	\
	 cur = tmp, tmp = cur->next)

enum DRI2FrameEventType {
    DRI2_SWAP,
    DRI2_WAITMSC,
};

typedef struct _DRI2FrameEvent {
    DrawablePtr		 pDraw;
    ScreenPtr		 pScreen;
    ClientPtr		 client;
    enum DRI2FrameEventType type;
    int			 frame;
    struct list		 link;
} DRI2FrameEventRec;

typedef struct _DRI2Screen {
    const char			*driverName;
    const char			*deviceName;
    int				 fd;
    unsigned int		 lastSequence;
    drmEventContext		 event_context;
    struct list			 swaps;

    DRI2CreateBufferProcPtr	 CreateBuffer;
    DRI2DestroyBufferProcPtr	 DestroyBuffer;
    DRI2CopyRegionProcPtr	 CopyRegion;
    DRI2SetupSwapProcPtr	 SetupSwap;
    DRI2SwapBuffersProcPtr	 SwapBuffers;
    DRI2GetMSCProcPtr		 GetMSC;
    DRI2SetupWaitMSCProcPtr	 SetupWaitMSC;

    HandleExposuresProcPtr       HandleExposures;
} DRI2ScreenRec;

static DRI2ScreenPtr
DRI2GetScreen(ScreenPtr pScreen)
{
    return dixLookupPrivate(&pScreen->devPrivates, dri2ScreenPrivateKey);
}

static DRI2DrawablePtr
DRI2GetDrawable(DrawablePtr pDraw)
{
    WindowPtr		  pWin;
    PixmapPtr		  pPixmap;

    if (!pDraw)
	return NULL;

    if (pDraw->type == DRAWABLE_WINDOW)
    {
	pWin = (WindowPtr) pDraw;
	return dixLookupPrivate(&pWin->devPrivates, dri2WindowPrivateKey);
    }
    else
    {
	pPixmap = (PixmapPtr) pDraw;
	return dixLookupPrivate(&pPixmap->devPrivates, dri2PixmapPrivateKey);
    }
}

int
DRI2CreateDrawable(DrawablePtr pDraw)
{
    WindowPtr	    pWin;
    PixmapPtr	    pPixmap;
    DRI2DrawablePtr pPriv;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv != NULL)
    {
	pPriv->refCount++;
	return Success;
    }

    pPriv = xalloc(sizeof *pPriv);
    if (pPriv == NULL)
	return BadAlloc;

    pPriv->refCount = 1;
    pPriv->width = pDraw->width;
    pPriv->height = pDraw->height;
    pPriv->buffers = NULL;
    pPriv->bufferCount = 0;
    pPriv->swapsPending = 0;
    pPriv->flipsPending = 0;
    pPriv->blockedClient = NULL;
    pPriv->swap_count = 0;
    pPriv->swap_interval = 1;
    pPriv->last_swap_target = 0;

    if (pDraw->type == DRAWABLE_WINDOW)
    {
	pWin = (WindowPtr) pDraw;
	dixSetPrivate(&pWin->devPrivates, dri2WindowPrivateKey, pPriv);
    }
    else
    {
	pPixmap = (PixmapPtr) pDraw;
	dixSetPrivate(&pPixmap->devPrivates, dri2PixmapPrivateKey, pPriv);
    }

    return Success;
}

static int
find_attachment(DRI2DrawablePtr pPriv, unsigned attachment)
{
    int i;

    if (pPriv->buffers == NULL) {
	return -1;
    }

    for (i = 0; i < pPriv->bufferCount; i++) {
	if ((pPriv->buffers[i] != NULL)
	    && (pPriv->buffers[i]->attachment == attachment)) {
	    return i;
	}
    }

    return -1;
}

static DRI2BufferPtr
allocate_or_reuse_buffer(DrawablePtr pDraw, DRI2ScreenPtr ds,
			 DRI2DrawablePtr pPriv,
			 unsigned int attachment, unsigned int format,
			 int dimensions_match)
{
    DRI2BufferPtr buffer;
    int old_buf;

    old_buf = find_attachment(pPriv, attachment);

    if ((old_buf < 0)
	|| !dimensions_match
	|| (pPriv->buffers[old_buf]->format != format)) {
	buffer = (*ds->CreateBuffer)(pDraw, attachment, format);
    } else {
	buffer = pPriv->buffers[old_buf];
	pPriv->buffers[old_buf] = NULL;
    }

    return buffer;
}

static DRI2BufferPtr *
do_get_buffers(DrawablePtr pDraw, int *width, int *height,
	       unsigned int *attachments, int count, int *out_count,
	       int has_format)
{
    DRI2ScreenPtr   ds = DRI2GetScreen(pDraw->pScreen);
    DRI2DrawablePtr pPriv = DRI2GetDrawable(pDraw);
    DRI2BufferPtr  *buffers;
    int need_real_front = 0;
    int need_fake_front = 0;
    int have_fake_front = 0;
    int front_format = 0;
    int dimensions_match;
    int i;

    if (!pPriv) {
	*width = pDraw->width;
	*height = pDraw->height;
	*out_count = 0;
	return NULL;
    }

    dimensions_match = (pDraw->width == pPriv->width)
	&& (pDraw->height == pPriv->height);

    buffers = xalloc((count + 1) * sizeof(buffers[0]));

    for (i = 0; i < count; i++) {
	const unsigned attachment = *(attachments++);
	const unsigned format = (has_format) ? *(attachments++) : 0;

	buffers[i] = allocate_or_reuse_buffer(pDraw, ds, pPriv, attachment,
					      format, dimensions_match);

	/* If the drawable is a window and the front-buffer is requested,
	 * silently add the fake front-buffer to the list of requested
	 * attachments.  The counting logic in the loop accounts for the case
	 * where the client requests both the fake and real front-buffer.
	 */
	if (attachment == DRI2BufferBackLeft) {
	    need_real_front++;
	    front_format = format;
	}

	if (attachment == DRI2BufferFrontLeft) {
	    need_real_front--;
	    front_format = format;

	    if (pDraw->type == DRAWABLE_WINDOW) {
		need_fake_front++;
	    }
	}

	if (pDraw->type == DRAWABLE_WINDOW) {
	    if (attachment == DRI2BufferFakeFrontLeft) {
		need_fake_front--;
		have_fake_front = 1;
	    }
	}
    }

    if (need_real_front > 0) {
	buffers[i++] = allocate_or_reuse_buffer(pDraw, ds, pPriv,
						DRI2BufferFrontLeft,
						front_format, dimensions_match);
    }

    if (need_fake_front > 0) {
	buffers[i++] = allocate_or_reuse_buffer(pDraw, ds, pPriv,
						DRI2BufferFakeFrontLeft,
						front_format, dimensions_match);
	have_fake_front = 1;
    }

    *out_count = i;


    if (pPriv->buffers != NULL) {
	for (i = 0; i < pPriv->bufferCount; i++) {
	    if (pPriv->buffers[i] != NULL) {
		(*ds->DestroyBuffer)(pDraw, pPriv->buffers[i]);
	    }
	}

	xfree(pPriv->buffers);
    }

    pPriv->buffers = buffers;
    pPriv->bufferCount = *out_count;
    pPriv->width = pDraw->width;
    pPriv->height = pDraw->height;
    *width = pPriv->width;
    *height = pPriv->height;


    /* If the client is getting a fake front-buffer, pre-fill it with the
     * contents of the real front-buffer.  This ensures correct operation of
     * applications that call glXWaitX before calling glDrawBuffer.
     */
    if (have_fake_front) {
	BoxRec box;
	RegionRec region;

	box.x1 = 0;
	box.y1 = 0;
	box.x2 = pPriv->width;
	box.y2 = pPriv->height;
	REGION_INIT(pDraw->pScreen, &region, &box, 0);

	DRI2CopyRegion(pDraw, &region, DRI2BufferFakeFrontLeft,
		       DRI2BufferFrontLeft);
    }

    return pPriv->buffers;
}

DRI2BufferPtr *
DRI2GetBuffers(DrawablePtr pDraw, int *width, int *height,
	       unsigned int *attachments, int count, int *out_count)
{
    return do_get_buffers(pDraw, width, height, attachments, count,
			  out_count, FALSE);
}

DRI2BufferPtr *
DRI2GetBuffersWithFormat(DrawablePtr pDraw, int *width, int *height,
			 unsigned int *attachments, int count, int *out_count)
{
    return do_get_buffers(pDraw, width, height, attachments, count,
			  out_count, TRUE);
}

int
DRI2CopyRegion(DrawablePtr pDraw, RegionPtr pRegion,
	       unsigned int dest, unsigned int src)
{
    DRI2ScreenPtr   ds = DRI2GetScreen(pDraw->pScreen);
    DRI2DrawablePtr pPriv;
    DRI2BufferPtr   pDestBuffer, pSrcBuffer;
    int		    i;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return BadDrawable;

    pDestBuffer = NULL;
    pSrcBuffer = NULL;
    for (i = 0; i < pPriv->bufferCount; i++)
    {
	if (pPriv->buffers[i]->attachment == dest)
	    pDestBuffer = (DRI2BufferPtr) pPriv->buffers[i];
	if (pPriv->buffers[i]->attachment == src)
	    pSrcBuffer = (DRI2BufferPtr) pPriv->buffers[i];
    }
    if (pSrcBuffer == NULL || pDestBuffer == NULL)
	return BadValue;

    (*ds->CopyRegion)(pDraw, pRegion, pDestBuffer, pSrcBuffer);

    return Success;
}

static Bool
DRI2FlipCheck(DrawablePtr pDraw)
{
    ScreenPtr pScreen = pDraw->pScreen;
    WindowPtr pWin, pRoot;
    PixmapPtr pWinPixmap, pRootPixmap;

    if (pDraw->type == DRAWABLE_PIXMAP)
	return TRUE;

    pRoot = WindowTable[pScreen->myNum];
    pRootPixmap = pScreen->GetWindowPixmap(pRoot);

    pWin = (WindowPtr) pDraw;
    pWinPixmap = pScreen->GetWindowPixmap(pWin);
    if (pRootPixmap != pWinPixmap)
	return FALSE;
    if (!REGION_EQUAL(pScreen, &pWin->clipList, &pRoot->winSize))
	return FALSE;

    return TRUE;
}

static Bool DRI2AddFrameEvent(DrawablePtr pDraw, ClientPtr client,
			      enum DRI2FrameEventType type, int frame)
{
    DRI2ScreenPtr   ds = DRI2GetScreen(pDraw->pScreen);
    DRI2DrawablePtr pPriv;
    DRI2FrameEventPtr new;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return FALSE;

    new = xcalloc(1, sizeof(DRI2FrameEventRec));
    if (!new)
	return FALSE;

    new->pScreen = pDraw->pScreen;
    new->pDraw = pDraw;
    new->client = client;
    new->frame = frame;
    new->type = type;

    list_add_tail(&new->link, &ds->swaps);

    return TRUE;
}

static void DRI2RemoveFrameEvent(DRI2FrameEventPtr event)
{
    list_del(&event->link);
    xfree(event);
}

static void
DRI2WaitMSCComplete(DRI2FrameEventPtr swap, unsigned int sequence,
		    unsigned int tv_sec, unsigned int tv_usec)
{
    DrawablePtr	    pDraw = swap->pDraw;
    DRI2DrawablePtr pPriv;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return;

    ProcDRI2WaitMSCReply(swap->client, ((CARD64)tv_sec * 1000000) + tv_usec,
			 sequence, pPriv->swap_count);

    if (pPriv->blockedClient)
	AttendClient(pPriv->blockedClient);

    pPriv->blockedClient = NULL;
}

/* Wake up clients waiting for flip/swap completion */
static void DRI2SwapComplete(DRI2DrawablePtr pPriv)
{
    pPriv->swap_count++;
}

static void
DRI2SwapSubmit(DRI2FrameEventPtr swap)
{
    DrawablePtr	    pDraw = swap->pDraw;
    ScreenPtr	    pScreen = swap->pScreen;
    DRI2ScreenPtr   ds = DRI2GetScreen(pScreen);
    DRI2DrawablePtr pPriv;
    DRI2BufferPtr   pDestBuffer, pSrcBuffer;
    BoxRec	    box;
    RegionRec	    region;
    int             ret, i;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return;

    if (pPriv->refCount == 0) {
	xfree(pPriv);
	return;
    }

    pDestBuffer = NULL;
    pSrcBuffer = NULL;
    for (i = 0; i < pPriv->bufferCount; i++)
    {
	if (pPriv->buffers[i]->attachment == DRI2BufferFrontLeft)
	    pDestBuffer = pPriv->buffers[i];
	if (pPriv->buffers[i]->attachment == DRI2BufferBackLeft)
	    pSrcBuffer = pPriv->buffers[i];
    }
    if (pSrcBuffer == NULL || pDestBuffer == NULL)
	return;

    /* Ask the driver for a flip */
    if (pPriv->flipsPending) {
	pPriv->flipsPending--;
	ret = (*ds->SwapBuffers)(pScreen, pDestBuffer, pSrcBuffer, pPriv);
	if (ret == TRUE)
	    return;

	pPriv->swapsPending++;
	/* Schedule a copy for the next frame here? */
    }

    /* Swaps need a copy when we get the vblank event */
    box.x1 = 0;
    box.y1 = 0;
    box.x2 = pPriv->width;
    box.y2 = pPriv->height;
    REGION_INIT(pScreen, &region, &box, 0);

    DRI2CopyRegion(pDraw, &region, DRI2BufferFrontLeft, DRI2BufferBackLeft);
    pPriv->swapsPending--;

    DRI2SwapComplete(pPriv);
}

static void drm_vblank_handler(int fd, unsigned int frame, unsigned int tv_sec,
			       unsigned int tv_usec, void *user_data)
{
    DRI2ScreenPtr ds = user_data;
    DRI2DrawablePtr pPriv;
    struct list *cur, *tmp;

    if (list_is_empty(&ds->swaps)) {
	ErrorF("tried to dequeue non-existent swap\n");
	return;
    }

    list_foreach_safe(cur, tmp, &ds->swaps) {
	DRI2FrameEventPtr swap = container_of(cur, DRI2FrameEventRec, link);

	if (swap->frame != frame)
	    continue;

	pPriv = DRI2GetDrawable(swap->pDraw);
	if (pPriv == NULL) {
	    DRI2RemoveFrameEvent(swap);
	    ErrorF("no dri2priv??\n");
	    continue; /* FIXME: check priv refcounting */
	}

	switch (swap->type) {
	case DRI2_SWAP:
	    DRI2SwapSubmit(swap);
	    break;
	case DRI2_WAITMSC:
	    DRI2WaitMSCComplete(swap, frame, tv_sec, tv_usec);
	    break;
	default:
	    /* Unknown type */
	    break;
	}
	DRI2RemoveFrameEvent(swap);
    }
}

static void
drm_wakeup_handler(pointer data, int err, pointer p)
{
    DRI2ScreenPtr ds = data;
    fd_set *read_mask = p;

    if (err >= 0 && FD_ISSET(ds->fd, read_mask))
	drmHandleEvent(ds->fd, &ds->event_context);
}

int
DRI2SwapBuffers(ClientPtr client, DrawablePtr pDraw, CARD64 target_msc,
		CARD64 divisor, CARD64 remainder, CARD64 *swap_target)
{
    DRI2ScreenPtr   ds = DRI2GetScreen(pDraw->pScreen);
    DRI2DrawablePtr pPriv;
    CARD64	    event_frame;
    int             ret;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return BadDrawable;

    /*
     * Swap target for this swap is last swap target + swap interval since
     * we have to account for the current swap count, interval, and the
     * number of pending swaps.
     */
    *swap_target = pPriv->last_swap_target + pPriv->swap_interval;

    if (DRI2FlipCheck(pDraw)) {
	/*
	 * Flips schedule for the next vblank, so we need to schedule them at
	 * frame - 1 to honor the swap interval.
	 */
	pPriv->flipsPending++;
	if (pPriv->swap_interval > 1) {
	    *swap_target = *swap_target - 1;
	    /* fixme: prevent cliprect changes between now and the flip */
	} else {
	    /* FIXME: perform immediate page flip */
	    DRI2SwapComplete(pPriv);
	    return 0;
	}
    } else {
	pPriv->swapsPending++;
    }

    ret = (*ds->SetupSwap)(pDraw, *swap_target, divisor, remainder, ds,
			   &event_frame);
    if (!ret)
	return BadDrawable;

    if (!DRI2AddFrameEvent(pDraw, client, DRI2_SWAP, event_frame))
	return BadValue;

    pPriv->last_swap_target = *swap_target;

    return Success;
}

Bool
DRI2WaitSwap(ClientPtr client, DrawablePtr pDrawable)
{
    DRI2DrawablePtr pPriv = DRI2GetDrawable(pDrawable);

    /* If we're currently waiting for a swap on this drawable, reset
     * the request and suspend the client.  We only support one
     * blocked client per drawable. */
    if ((pPriv->swapsPending || pPriv->flipsPending) &&
	pPriv->blockedClient == NULL) {
	ResetCurrentRequest(client);
	client->sequence--;
	IgnoreClient(client);
	pPriv->blockedClient = client;
	return TRUE;
    }

    return FALSE;
}

void
DRI2SwapInterval(DrawablePtr pDrawable, int interval)
{
    DRI2DrawablePtr pPriv = DRI2GetDrawable(pDrawable);

    /* fixme: check against arbitrary max? */

    pPriv->swap_interval = interval;
}



int
DRI2GetMSC(DrawablePtr pDraw, CARD64 *ust, CARD64 *msc, CARD64 *sbc)
{
    DRI2ScreenPtr ds = DRI2GetScreen(pDraw->pScreen);
    DRI2DrawablePtr pPriv;
    Bool ret;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return BadDrawable;

    if (!ds->GetMSC)
	FatalError("advertised MSC support w/o driver hook\n");

    ret = (*ds->GetMSC)(pDraw, ust, msc);
    if (!ret)
	return BadDrawable;

    *sbc = pPriv->swap_count;

    return Success;
}

int
DRI2WaitMSC(ClientPtr client, DrawablePtr pDraw, CARD64 target_msc,
	    CARD64 divisor, CARD64 remainder)
{
    DRI2ScreenPtr ds = DRI2GetScreen(pDraw->pScreen);
    DRI2DrawablePtr pPriv;
    CARD64 event_frame;
    Bool ret;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return BadDrawable;

    ret = (*ds->SetupWaitMSC)(pDraw, target_msc, divisor, remainder, ds,
			      &event_frame);
    if (!ret) {
	ErrorF("setupmsc failed: %d\n", ret);
	return BadDrawable;
    }

    ret = DRI2AddFrameEvent(pDraw, client, DRI2_WAITMSC, event_frame);
    if (!ret)
	return BadDrawable;

    /* DDX returned > 0, block the client until its wait completes */

    if (pPriv->blockedClient == NULL) {
	IgnoreClient(client);
	pPriv->blockedClient = client;
    }

    return Success;
}

int
DRI2WaitSBC(DrawablePtr pDraw, CARD64 target_sbc, CARD64 *ust, CARD64 *msc,
	    CARD64 *sbc)
{
    DRI2DrawablePtr pPriv;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return BadDrawable;

    /* fixme: put client to sleep until swap count hits target */

    return Success;
}

void
DRI2DestroyDrawable(DrawablePtr pDraw)
{
    DRI2ScreenPtr   ds = DRI2GetScreen(pDraw->pScreen);
    DRI2DrawablePtr pPriv;
    WindowPtr  	    pWin;
    PixmapPtr	    pPixmap;

    pPriv = DRI2GetDrawable(pDraw);
    if (pPriv == NULL)
	return;

    pPriv->refCount--;
    if (pPriv->refCount > 0)
	return;

    if (pPriv->buffers != NULL) {
	int i;

	for (i = 0; i < pPriv->bufferCount; i++)
	    (*ds->DestroyBuffer)(pDraw, pPriv->buffers[i]);

	xfree(pPriv->buffers);
    }

    /* If the window is destroyed while we have a swap pending, don't
     * actually free the priv yet.  We'll need it in the DRI2SwapComplete()
     * callback and we'll free it there once we're done. */
    if (!pPriv->swapsPending && !pPriv->flipsPending)
	xfree(pPriv);

    if (pDraw->type == DRAWABLE_WINDOW)
    {
	pWin = (WindowPtr) pDraw;
	dixSetPrivate(&pWin->devPrivates, dri2WindowPrivateKey, NULL);
    }
    else
    {
	pPixmap = (PixmapPtr) pDraw;
	dixSetPrivate(&pPixmap->devPrivates, dri2PixmapPrivateKey, NULL);
    }
}

Bool
DRI2Connect(ScreenPtr pScreen, unsigned int driverType, int *fd,
	    const char **driverName, const char **deviceName)
{
    DRI2ScreenPtr ds = DRI2GetScreen(pScreen);

    if (ds == NULL)
	return FALSE;

    if (driverType != DRI2DriverDRI)
	return BadValue;

    *fd = ds->fd;
    *driverName = ds->driverName;
    *deviceName = ds->deviceName;

    return TRUE;
}

Bool
DRI2Authenticate(ScreenPtr pScreen, drm_magic_t magic)
{
    DRI2ScreenPtr ds = DRI2GetScreen(pScreen);

    if (ds == NULL || drmAuthMagic(ds->fd, magic))
	return FALSE;

    return TRUE;
}

Bool
DRI2ScreenInit(ScreenPtr pScreen, DRI2InfoPtr info)
{
    DRI2ScreenPtr ds;

    if (info->version < 3)
	return FALSE;

    if (!xf86VGAarbiterAllowDRI(pScreen)) {
        xf86DrvMsg(pScreen->myNum, X_WARNING,
                  "[DRI2] Direct rendering is not supported when VGA arb is necessary for the device\n");
        return FALSE;
    }

    ds = xcalloc(1, sizeof *ds);
    if (!ds)
	return FALSE;

    ds->fd	       = info->fd;
    ds->driverName     = info->driverName;
    ds->deviceName     = info->deviceName;

    ds->CreateBuffer   = info->CreateBuffer;
    ds->DestroyBuffer  = info->DestroyBuffer;
    ds->CopyRegion     = info->CopyRegion;

    if (info->version >= 4) {
	ds->SetupSwap = info->SetupSwap;
	ds->SwapBuffers = info->SwapBuffers;
	ds->SetupWaitMSC = info->SetupWaitMSC;
	ds->GetMSC = info->GetMSC;
    }

    ds->event_context.version = DRM_EVENT_CONTEXT_VERSION;
    ds->event_context.vblank_handler = drm_vblank_handler;

    list_init(&ds->swaps);

    AddGeneralSocket(ds->fd);
    RegisterBlockAndWakeupHandlers((BlockHandlerProcPtr)NoopDDA,
				   drm_wakeup_handler, ds);

    dixSetPrivate(&pScreen->devPrivates, dri2ScreenPrivateKey, ds);

    xf86DrvMsg(pScreen->myNum, X_INFO, "[DRI2] Setup complete\n");

    return TRUE;
}

void
DRI2CloseScreen(ScreenPtr pScreen)
{
    DRI2ScreenPtr ds = DRI2GetScreen(pScreen);

    xfree(ds);
    dixSetPrivate(&pScreen->devPrivates, dri2ScreenPrivateKey, NULL);
}

extern ExtensionModule dri2ExtensionModule;

static pointer
DRI2Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone)
    {
	setupDone = TRUE;
	LoadExtension(&dri2ExtensionModule, FALSE);
    }
    else
    {
	if (errmaj)
	    *errmaj = LDR_ONCEONLY;
    }

    return (pointer) 1;
}

static XF86ModuleVersionInfo DRI2VersRec =
{
    "dri2",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    1, 1, 0,
    ABI_CLASS_EXTENSION,
    ABI_EXTENSION_VERSION,
    MOD_CLASS_NONE,
    { 0, 0, 0, 0 }
};

_X_EXPORT XF86ModuleData dri2ModuleData = { &DRI2VersRec, DRI2Setup, NULL };

void
DRI2Version(int *major, int *minor)
{
    if (major != NULL)
	*major = DRI2VersRec.majorversion;

    if (minor != NULL)
	*minor = DRI2VersRec.minorversion;
}
