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
#include "glitz_glucose.h"

extern __GLXcontext *__glXLastContext;

static glitz_glucose_drawable_t *
_glitz_glucose_create_drawable (glitz_glucose_screen_info_t *screen_info,
			    glitz_glucose_context_t     *context,
			    glitz_drawable_format_t *format,
			    __GLXdrawable           *glx_drawable,
			    int                     width,
			    int                     height)
{
    glitz_glucose_drawable_t *drawable;

    drawable = (glitz_glucose_drawable_t *) malloc (sizeof (glitz_glucose_drawable_t));
    if (drawable == NULL)
	return NULL;

    drawable->screen_info = screen_info;
    drawable->context = context;
    drawable->drawable = glx_drawable;
    drawable->width = width;
    drawable->height = height;

    _glitz_drawable_init (&drawable->base,
			  &screen_info->formats[format->id],
			  &context->backend,
			  width, height);

    if (!context->initialized) {
	glitz_glucose_push_current (drawable, NULL, GLITZ_CONTEXT_CURRENT, NULL);
	glitz_glucose_pop_current (drawable);
    }

    if (width > context->backend.max_viewport_dims[0] ||
	height > context->backend.max_viewport_dims[1]) {
	free (drawable);
	return NULL;
    }

    screen_info->drawables++;

    return drawable;
}

glitz_bool_t
_glitz_glucose_drawable_update_size (glitz_glucose_drawable_t *drawable,
				 int                  width,
				 int                  height)
{
    drawable->width  = width;
    drawable->height = height;

    return 1;
}

glitz_drawable_t *
glitz_glucose_create_drawable_for_window (__GLXscreen        *screen,
				      glitz_drawable_format_t *format,
				      __GLXdrawable           *window,
				      unsigned int            width,
				      unsigned int            height)
{
    glitz_glucose_drawable_t        *drawable;
    glitz_glucose_screen_info_t     *screen_info;
    glitz_glucose_context_t         *context;
    glitz_int_drawable_format_t *iformat;

    screen_info = glitz_glucose_screen_info_get (screen);
    if (!screen_info)
	return NULL;

    if (format->id >= screen_info->n_formats)
	return NULL;

    iformat = &screen_info->formats[format->id];
    if (!(iformat->types & GLITZ_DRAWABLE_TYPE_WINDOW_MASK))
	return NULL;

    context = glitz_glucose_context_get (screen_info, format);
    if (!context)
	return NULL;

    drawable = _glitz_glucose_create_drawable (screen_info, context, format,
					   window, 
					   width, height);
    if (!drawable)
	return NULL;

    return &drawable->base;
}

void
glitz_glucose_destroy (void *abstract_drawable)
{
    glitz_glucose_drawable_t *drawable = (glitz_glucose_drawable_t *)
	abstract_drawable;

    drawable->screen_info->drawables--;
    if (drawable->screen_info->drawables == 0) {
	/*
	 * Last drawable? We have to destroy all fragment programs as this may
	 * be our last chance to have a context current.
	 */
	glitz_glucose_push_current (abstract_drawable, NULL,
				GLITZ_CONTEXT_CURRENT, NULL);
	glitz_program_map_fini (drawable->base.backend->gl,
				&drawable->screen_info->program_map);
	glitz_program_map_init (&drawable->screen_info->program_map);
	glitz_glucose_pop_current (abstract_drawable);
    }

    if (__glXLastContext->drawPriv == drawable->drawable) {
        __glXDeassociateContext(drawable->context->context);
	__glXLastContext = NULL;
    }

    free (drawable);
}

glitz_bool_t
glitz_glucose_swap_buffers (void *abstract_drawable)
{
    glitz_glucose_drawable_t *drawable = (glitz_glucose_drawable_t *)
	abstract_drawable;

    drawable->drawable->swapBuffers (drawable->drawable);

    return 1;
}

glitz_bool_t
glitz_glucose_copy_sub_buffer (void *abstract_drawable,
			   int  x,
			   int  y,
			   int  width,
			   int  height)
{
    glitz_glucose_drawable_t    *drawable = (glitz_glucose_drawable_t *)
	abstract_drawable;

    drawable->drawable->copySubBuffer (drawable->drawable, x, y, width, height);

    return 1;
}
