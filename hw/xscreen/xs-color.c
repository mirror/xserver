#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_aux.h>
#include "scrnintstr.h"
#include "window.h"
#include "windowstr.h"
#include "colormapst.h"
#include "resource.h"

#include "xs-globals.h"
#include "xs-visual.h"
#include "xs-color.h"

static ColormapPtr xsInstalledMap;

/**
 * Creates a colormap on the backing server.
 * FIXME: Do I need to actually initialize the values in it?
 * Xnest does, Xdmx doesn't seem to.
 **/
Bool xsCreateColormap(ColormapPtr pCmap)
{
    XCBVISUALID vid;
    VisualPtr pVis;

    pVis = pCmap->pVisual;
    pCmap->devPriv = xalloc(sizeof(XscreenPrivColormap));
    XS_CMAP_PRIV(pCmap)->colormap = XCBCOLORMAPNew(xsConnection);
    vid = xsGetVisual(pVis)->visual_id;   
    XCBCreateColormap(xsConnection,
                      (pVis->class & DynamicClass) ?  XCBColormapAllocAll : XCBColormapAllocNone,
                      XS_CMAP_PRIV(pCmap)->colormap,
                      xsBackingRoot.window,
                      vid);
}

/**
 * Frees a colormap on the backing server and deallocates it's privates.
 **/
void xsDestroyColormap(ColormapPtr pCmap)
{
    XCBFreeColormap(xsConnection, XS_CMAP_PRIV(pCmap)->colormap);
    xfree(pCmap->devPriv);
}


void xsSetInstalledColormapWindows(ScreenPtr pScreen)
{
    /*FIXME. Actually implement something here.*/
}

void xsDirectUninstallColormaps(ScreenPtr pScreen)
{
    int i, n;
    XCBCOLORMAP pCmapIDs[MAXCMAPS];

    /*do I want this? What does it do?
    if (!xsDoDirectColormaps) 
        return;
    */
    n = (*pScreen->ListInstalledColormaps)(pScreen, (XID*)pCmapIDs);

    for (i = 0; i < n; i++) {
        ColormapPtr pCmap;

        pCmap = (ColormapPtr)LookupIDByType(pCmapIDs[i].xid, RT_COLORMAP);
        if (pCmap)
            XCBUninstallColormap(xsConnection, XS_CMAP_PRIV(pCmap)->colormap);
    }
}
void xsInstallColormap(ColormapPtr pCmap)
{
    int index;

    if(pCmap != xsInstalledMap)
    {
        xsDirectUninstallColormaps(pCmap->pScreen);

        /* Uninstall pInstalledMap. Notify all interested parties. */
        if(xsInstalledMap != (ColormapPtr)None)
            WalkTree(pCmap->pScreen, TellLostMap, (pointer)&xsInstalledMap->mid);

        xsInstalledMap = pCmap;
        WalkTree(pCmap->pScreen, TellGainedMap, (pointer)&pCmap->mid);

        xsSetInstalledColormapWindows(pCmap->pScreen);
        //xsDirectInstallColormaps(pCmap->pScreen);
    }
}

void xsUninstallColormap(ColormapPtr pCmap)
{
    int index;

    if(pCmap == xsInstalledMap)
    {
        if (pCmap->mid != pCmap->pScreen->defColormap)
        {
            xsInstalledMap = (ColormapPtr)LookupIDByType(pCmap->pScreen->defColormap,
                    RT_COLORMAP);
            (*pCmap->pScreen->InstallColormap)(xsInstalledMap);
        }
    }
}

void xsStoreColors(ColormapPtr pCmap, int nColors, XCBCOLORITEM *pColors)
{
}

void xsResolveColor(CARD16 *r, CARD16 *g, CARD16 *b, VisualPtr pVisual)
{
}

int xsListInstalledColormaps(ScreenPtr pScreen, XCBCOLORMAP *pCmapIDs)
{
    if (xsInstalledMap) {
        pCmapIDs->xid = xsInstalledMap->mid;
        return 1;
    }
    else
        return 0;
}
