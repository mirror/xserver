/*
 * Copyright © 2005 Novell, Inc.
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

#include "xglglx.h"

#ifdef GLXEXT

#ifdef XGL_MODULAR
#include <dlfcn.h>
#endif

xglGLXFuncRec __xglGLXFunc;

#ifndef NGLXEXTLOG
FILE *__xglGLXLogFp;
#endif

static void *glXHandle = 0;

#define SYM(ptr, name) { (void **) &(ptr), (name) }

__GLXprovider *__xglMesaProvider;

void *(*__GlxGetMesaProvider) (void);

void
GlxSetVisualConfigs (int	       nconfigs,
		     __GLXvisualConfig *configs,
		     void              **privates)
{
    if (glXHandle)
	(*__xglGLXFunc.setVisualConfigs) (nconfigs, configs, privates);
}

void
GlxExtensionInit (void)
{
    if (glXHandle)
	(*__xglGLXFunc.extensionInit) ();
}

void
GlxWrapInitVisuals (miInitVisualsProcPtr *initVisuals)
{
    if (glXHandle)
	(*__xglGLXFunc.wrapInitVisuals) (initVisuals);
}

int
GlxInitVisuals (VisualPtr     *visualp,
		DepthPtr      *depthp,
		int	      *nvisualp,
		int	      *ndepthp,
		int	      *rootDepthp,
		VisualID      *defaultVisp,
		unsigned long sizes,
		int	      bitsPerRGB,
		int	      preferredVis)
{
    if (glXHandle)
	return (*__xglGLXFunc.initVisuals) (visualp, depthp, nvisualp, ndepthp,
					    rootDepthp, defaultVisp, sizes,
					    bitsPerRGB, preferredVis);

    return 0;
}

void
GlxFlushContextCache (void)
{
    if (glXHandle)
	(*__xglGLXFunc.flushContextCache) ();
}

void
GlxSetRenderTables (struct _glapi_table *table)
{
    if (glXHandle)
	(*__xglGLXFunc.setRenderTables) (table);
}

void
GlxPushProvider (__GLXprovider *provider)
{
    if (glXHandle)
	(*__xglGLXFunc.pushProvider) (provider);
}

void
GlxScreenInit (__GLXscreen *screen,
	       ScreenPtr   pScreen)
{
    if (glXHandle)
	(*__xglGLXFunc.screenInit) (screen, pScreen);
}

void
GlxScreenDestroy (__GLXscreen *screen)
{
    if (glXHandle)
	(*__xglGLXFunc.screenDestroy) (screen);
}

GLboolean
GlxDrawableInit (__GLXdrawable *drawable,
		 __GLXcontext  *ctx,
		 DrawablePtr   pDrawable,
		 XID	       drawId)
{
    if (glXHandle)
	return (*__xglGLXFunc.drawableInit) (drawable, ctx, pDrawable, drawId);

    return GL_FALSE;
}

Bool
xglLoadGLXModules (void)
{

#ifdef XGL_MODULAR
    if (!glXHandle)
    {
	xglSymbolRec sym[] = {
	    SYM (__xglGLXFunc.extensionInit,     "GlxExtensionInit"),
	    SYM (__xglGLXFunc.setVisualConfigs,  "GlxSetVisualConfigs"),
	    SYM (__xglGLXFunc.wrapInitVisuals,   "GlxWrapInitVisuals"),
	    SYM (__xglGLXFunc.initVisuals,	 "GlxInitVisuals"),
	    SYM (__xglGLXFunc.flushContextCache, "__glXFlushContextCache"),
	    SYM (__xglGLXFunc.setRenderTables,   "GlxSetRenderTables"),
	    SYM (__xglGLXFunc.pushProvider,      "GlxPushProvider"),
	    SYM (__xglGLXFunc.screenInit,        "__glXScreenInit"),
	    SYM (__xglGLXFunc.screenDestroy,     "__glXScreenDestroy"),
	    SYM (__xglGLXFunc.drawableInit,      "__glXDrawableInit"),

	    SYM (__GlxGetMesaProvider, "GlxGetMesaProvider")
	};

	glXHandle = xglLoadModule ("glxext", RTLD_NOW | RTLD_LOCAL);
	if (!glXHandle)
	    return FALSE;

	if (!xglLookupSymbols (glXHandle, sym, sizeof (sym) / sizeof (sym[0])))
	{
	    xglUnloadModule (glXHandle);
	    glXHandle = 0;

	    return FALSE;
	}

	__xglMesaProvider = __GlxGetMesaProvider ();

	if (!xglLoadHashFuncs (glXHandle))
	{
	    xglUnloadModule (glXHandle);
	    glXHandle = 0;

	    return FALSE;
	}
    }

    return TRUE;
#else
    return FALSE;
#endif

}

void
xglUnloadGLXModules (void)
{

#ifdef XGL_MODULAR
    if (glXHandle)
    {
	xglUnloadModule (glXHandle);
	glXHandle = 0;
    }
#endif

}

#endif
