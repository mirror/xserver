#ifdef HAVE_XS_CONFIG_H
#include <xs-config.h>
#endif
#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_image.h>
#include "regionstr.h"
#include "gcstruct.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "region.h"
#include "servermd.h"


#include "xs-globals.h"
#include "xs-window.h"

void xsDoConfigure(XCBConfigureNotifyEvent *e)
{
}

void xsHandleEvent(XCBGenericEvent *evt)
{
    switch (evt->response_type & ~0x80)
    {
        case XCBConfigureNotify:
            xsDoConfigure((XCBConfigureNotifyEvent *)evt);
            break;
        default:
            ErrorF("Warning: Unhandled Event");
    }
}
