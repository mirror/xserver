/*
 * Copyright © 2004 David Reveman
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
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 *
 * This file has been modified from the GLitz GLX code for use with glucose
 * by: Alan Hourihane <alanh@tungstengraphics.com>
 * 
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "dix-config.h"
#endif

#include "glxserver.h"
#include "glcontextmodes.h"
#include "glitz.h"
#include "glitz_glucose.h"
#include "glthread.h"
#include "dispatch.h"
#include "glapi.h"
#include <stdlib.h>

extern __GLXcontext *__glXLastContext;
extern glitz_gl_proc_address_list_t _glitz_glucose_gl_proc_address;

static void
_glitz_glucose_context_create (glitz_glucose_screen_info_t *screen_info,
			   int                     visualid,
			   __GLXcontext            *share_list,
			   glitz_glucose_context_t     *context)
{
    __GLXscreen *screen = screen_info->display_info->display;
    __GLcontextModes *mode;

    mode = _gl_context_modes_find_visual(screen->modes, visualid);

    context->context = screen->createContext (screen, mode, share_list);
    context->id = visualid;
}

static glitz_context_t *
_glitz_glucose_create_context (void                    *abstract_drawable,
			   glitz_drawable_format_t *format)
{
    glitz_glucose_drawable_t   *drawable = (glitz_glucose_drawable_t *)
	abstract_drawable;
    glitz_glucose_screen_info_t *screen_info = drawable->screen_info;
    unsigned long	    format_id =
	screen_info->formats[format->id].u.uval;
    glitz_glucose_context_t	    *context;

    context = malloc (sizeof (glitz_glucose_context_t));
    if (!context)
	return NULL;

    _glitz_context_init (&context->base, &drawable->base);

    _glitz_glucose_context_create (screen_info,
				   format_id,
				   screen_info->root_context,
				   context);

    return (glitz_context_t *) context;
}

static void
_glitz_glucose_context_destroy (void *abstract_context)
{
    glitz_glucose_context_t *context = (glitz_glucose_context_t *) abstract_context;
    glitz_glucose_drawable_t *drawable = (glitz_glucose_drawable_t *)
	context->base.drawable;

    if (drawable->screen_info->display_info->thread_info->cctx ==
	&context->base)
    {
	__glXDeassociateContext(context->context);
	__glXLastContext = NULL;

	drawable->screen_info->display_info->thread_info->cctx = NULL;
    }

    context->context->destroy(context->context);

    _glitz_context_fini (&context->base);

    free (context);
}

static void
_glitz_glucose_copy_context (void          *abstract_src,
			 void          *abstract_dst,
			 unsigned long mask)
{
    glitz_glucose_context_t  *src = (glitz_glucose_context_t *) abstract_src;
    glitz_glucose_context_t  *dst = (glitz_glucose_context_t *) abstract_dst;

    src->context->copy(dst->context, src->context, mask);
}

static void
_glitz_glucose_make_current (void *abstract_drawable,
			 void *abstract_context)
{
    glitz_glucose_context_t  *context = (glitz_glucose_context_t *) abstract_context;
    glitz_glucose_drawable_t *drawable = (glitz_glucose_drawable_t *)
	abstract_drawable;
    glitz_glucose_display_info_t *display_info =
	drawable->screen_info->display_info;

    if (drawable->base.width  != drawable->width ||
	drawable->base.height != drawable->height)
	_glitz_glucose_drawable_update_size (drawable,
					 drawable->base.width,
					 drawable->base.height);

    if ((__glXLastContext != context->context) ||
	(__glXLastContext->drawPriv != drawable->drawable))
    {
	if (display_info->thread_info->cctx)
	{
	    glitz_context_t *ctx = display_info->thread_info->cctx;

	    if (ctx->lose_current)
		ctx->lose_current (ctx->closure);
	}

	context->context->drawPriv = drawable->drawable;
	context->context->readPriv = drawable->drawable;
        context->context->makeCurrent( context->context );
	__glXAssociateContext(context->context);
	__glXLastContext = context->context;
        context->context->isCurrent = TRUE;
    }

    display_info->thread_info->cctx = &context->base;
}

static void
_glitz_glucose_notify_dummy (void            *abstract_drawable,
			 glitz_surface_t *surface) {}

static glitz_function_pointer_t
_glitz_glucose_context_get_proc_address (void       *abstract_context,
				     const char *name)
{
    glitz_glucose_context_t  *context = (glitz_glucose_context_t *) abstract_context;
    glitz_glucose_drawable_t *drawable = (glitz_glucose_drawable_t *)
	context->base.drawable;

    return glitz_glucose_get_proc_address (name, drawable->screen_info);
}

glitz_glucose_context_t *
glitz_glucose_context_get (glitz_glucose_screen_info_t *screen_info,
		       glitz_drawable_format_t *format)
{
    glitz_glucose_context_t *context;
    glitz_glucose_context_t **contexts = screen_info->contexts;
    int index, n_contexts = screen_info->n_contexts;
    unsigned long format_id;

    for (; n_contexts; n_contexts--, contexts++)
	if ((*contexts)->id == screen_info->formats[format->id].u.uval)
	    return *contexts;

    index = screen_info->n_contexts++;

    screen_info->contexts =
	realloc (screen_info->contexts,
		 sizeof (glitz_glucose_context_t *) * screen_info->n_contexts);
    if (!screen_info->contexts)
	return NULL;

    context = malloc (sizeof (glitz_glucose_context_t));
    if (!context)
	return NULL;

    screen_info->contexts[index] = context;

    format_id = screen_info->formats[format->id].u.uval;

    _glitz_glucose_context_create (screen_info,
				   format_id,
				   screen_info->root_context,
				   context);

    if (!screen_info->root_context)
	screen_info->root_context = context->context;

    context->backend.gl = &_glitz_glucose_gl_proc_address;

    context->backend.create_pbuffer = NULL;
    context->backend.destroy = glitz_glucose_destroy;
    context->backend.push_current = glitz_glucose_push_current;
    context->backend.pop_current = glitz_glucose_pop_current;
    context->backend.attach_notify = _glitz_glucose_notify_dummy;
    context->backend.detach_notify = _glitz_glucose_notify_dummy;
    context->backend.swap_buffers = glitz_glucose_swap_buffers;
    context->backend.copy_sub_buffer = glitz_glucose_copy_sub_buffer;

    context->backend.create_context = _glitz_glucose_create_context;
    context->backend.destroy_context = _glitz_glucose_context_destroy;
    context->backend.copy_context = _glitz_glucose_copy_context;
    context->backend.make_current = _glitz_glucose_make_current;
    context->backend.get_proc_address = _glitz_glucose_context_get_proc_address;

    context->backend.draw_buffer = _glitz_drawable_draw_buffer;
    context->backend.read_buffer = _glitz_drawable_read_buffer;

    context->backend.drawable_formats = NULL;
    context->backend.n_drawable_formats = 0;

    if (screen_info->n_formats)
    {
	int size;

	size = sizeof (glitz_int_drawable_format_t) * screen_info->n_formats;
	context->backend.drawable_formats = malloc (size);
	if (context->backend.drawable_formats)
	{
	    memcpy (context->backend.drawable_formats, screen_info->formats,
		    size);
	    context->backend.n_drawable_formats = screen_info->n_formats;
	}
    }

    context->backend.texture_formats = NULL;
    context->backend.formats = NULL;
    context->backend.n_formats = 0;

    context->backend.program_map = &screen_info->program_map;
    context->backend.feature_mask = 0;

    context->initialized = 0;

    return context;
}

void
glitz_glucose_context_destroy (glitz_glucose_screen_info_t *screen_info,
			   glitz_glucose_context_t     *context)
{
    if (context->backend.drawable_formats)
	free (context->backend.drawable_formats);

    if (context->backend.formats)
	free (context->backend.formats);

    if (context->backend.texture_formats)
	free (context->backend.texture_formats);

    context->context->destroy (context->context);

    free(context);
}

static void
_glitz_glucose_context_initialize (glitz_glucose_screen_info_t *screen_info,
			       glitz_glucose_context_t     *context)
{
    glitz_backend_init (&context->backend,
			glitz_glucose_get_proc_address,
			(void *) screen_info);

    glitz_initiate_state (&_glitz_glucose_gl_proc_address);

    context->initialized = 1;
}

static void
_glitz_glucose_context_make_current (glitz_glucose_drawable_t *drawable,
				 glitz_bool_t         finish)
{
    glitz_glucose_display_info_t *display_info =
	drawable->screen_info->display_info;
    GLenum err;

    if (finish)
    {
	CALL_Finish ( GET_DISPATCH(), () );
	drawable->base.finished = 1;
    }

    if (display_info->thread_info->cctx)
    {
	glitz_context_t *ctx = display_info->thread_info->cctx;

	if (ctx->lose_current)
	    ctx->lose_current (ctx->closure);

	display_info->thread_info->cctx = NULL;
    }

    drawable->context->context->drawPriv = drawable->drawable;
    drawable->context->context->readPriv = drawable->drawable;
    err = drawable->context->context->makeCurrent(drawable->context->context);
    __glXAssociateContext(drawable->context->context);
    __glXLastContext = drawable->context->context;
    drawable->context->context->isCurrent = TRUE;

    drawable->base.update_all = 1;

    if (!drawable->context->initialized)
	_glitz_glucose_context_initialize (drawable->screen_info,
				       drawable->context);
}

static void
_glitz_glucose_context_update (glitz_glucose_drawable_t *drawable,
			   glitz_constraint_t   constraint,
			   glitz_bool_t         *restore_state)
{
    glitz_glucose_display_info_t *dinfo = drawable->screen_info->display_info;
    __GLXcontext *context = NULL;

    if (restore_state && constraint == GLITZ_ANY_CONTEXT_CURRENT)
    {
	if (dinfo->thread_info->cctx)
	{
	    *restore_state = 1;
	    return;
	}
    }

    drawable->base.flushed = drawable->base.finished = 0;

    switch (constraint) {
    case GLITZ_NONE:
	break;
    case GLITZ_ANY_CONTEXT_CURRENT:
	if (!dinfo->thread_info->cctx)
	    context = __glXLastContext;

	if (!context)
	    _glitz_glucose_context_make_current (drawable, 0);
	break;
    case GLITZ_CONTEXT_CURRENT:
	if (!dinfo->thread_info->cctx)
	    context = __glXLastContext;

	if (context != drawable->context->context)
	    _glitz_glucose_context_make_current (drawable, (context)? 1: 0);
	break;
    case GLITZ_DRAWABLE_CURRENT:
	if (drawable->base.width  != drawable->width ||
	    drawable->base.height != drawable->height)
	    _glitz_glucose_drawable_update_size (drawable,
					     drawable->base.width,
					     drawable->base.height);

	if (!dinfo->thread_info->cctx)
	    context = __glXLastContext;

	if ((context != drawable->context->context) ||
	    (__glXLastContext->drawPriv != drawable->drawable))
	    _glitz_glucose_context_make_current (drawable, (context)? 1: 0);
	break;
    }
}

glitz_bool_t
glitz_glucose_push_current (void               *abstract_drawable,
			glitz_surface_t    *surface,
			glitz_constraint_t constraint,
			glitz_bool_t       *restore_state)
{
    glitz_glucose_drawable_t *drawable = (glitz_glucose_drawable_t *)
	abstract_drawable;
    glitz_glucose_context_info_t *context_info;
    int index;

    if (restore_state)
	*restore_state = 0;

    index = drawable->screen_info->context_stack_size++;

    if (index > GLITZ_CONTEXT_STACK_SIZE)
    	FatalError("glitz context stack failure\n");

    context_info = &drawable->screen_info->context_stack[index];
    context_info->drawable = drawable;
    context_info->surface = surface;
    context_info->constraint = constraint;

    _glitz_glucose_context_update (context_info->drawable, constraint,
			       restore_state);

    return 1;
}

glitz_surface_t *
glitz_glucose_pop_current (void *abstract_drawable)
{
    glitz_glucose_drawable_t *drawable = (glitz_glucose_drawable_t *)
	abstract_drawable;
    glitz_glucose_context_info_t *context_info = NULL;
    int index;

    drawable->screen_info->context_stack_size--;
    index = drawable->screen_info->context_stack_size - 1;

    context_info = &drawable->screen_info->context_stack[index];

    if (context_info->drawable)
	_glitz_glucose_context_update (context_info->drawable,
				   context_info->constraint,
				   NULL);

    if (context_info->constraint == GLITZ_DRAWABLE_CURRENT)
	return context_info->surface;

    return NULL;
}
