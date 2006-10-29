#ifdef HAVE_XS_CONFIG_H
#include <xs-config.h>
#endif
#include <X11/Xmd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/xcb_image.h>
#include "regionstr.h"
#include "gcstruct.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "region.h"
#include "servermd.h"


#include "xs-globals.h"
#include "xs-window.h"

void xsDoConfigure(xcb_configure_notify_event_t *e)
{
}

void xsHandleEvent(xcb_generic_event_t *evt)
{
    switch (evt->response_type & ~0x80)
    {
        case XCB_CONFIGURE_NOTIFY:
            xsDoConfigure((xcb_configure_notify_event_t *)evt);
            break;
        default:
            ErrorF("Warning: Unhandled Event");
    }
}
