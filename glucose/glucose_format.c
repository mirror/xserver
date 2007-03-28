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
#include "dix-config.h"
#endif

#if 0
#include "glxserver.h"
#else
#include "scrnintstr.h"
#include "window.h"
#include "pixmap.h"
#include <GL/glx.h>
#include <GL/glxint.h>
/* For glxscreens.h */
typedef struct __GLXdrawable __GLXdrawable;
typedef struct __GLXcontext __GLXcontext;

#include "glxscreens.h"
#include "glxdrawable.h"
#include "glxcontext.h"
#endif
#include "glcontextmodes.h"
#include "glitz_glucose.h"

#include <stdlib.h>
#include <string.h>

static int
_glitz_glucose_format_compare (const void *elem1,
			   const void *elem2)
{
    glitz_int_drawable_format_t *format[2];
    int				i, score[2];

    format[0] = (glitz_int_drawable_format_t *) elem1;
    format[1] = (glitz_int_drawable_format_t *) elem2;
    i = score[0] = score[1] = 0;

    for (; i < 2; i++)
    {
	if (format[i]->d.color.fourcc != GLITZ_FOURCC_RGB)
	    score[i] -= 1000;

	if (format[i]->d.color.red_size)
	{
	    if (format[i]->d.color.red_size >= 8)
		score[i] += 5;

	    score[i] += 10;
	}

	if (format[i]->d.color.alpha_size)
	{
	    if (format[i]->d.color.alpha_size >= 8)
		score[i] += 5;

	    score[i] += 10;
	}

	if (format[i]->d.stencil_size)
	    score[i] += 5;

	if (format[i]->d.depth_size)
	    score[i] += 5;

	if (format[i]->d.doublebuffer)
	    score[i] += 10;

	if (format[i]->d.samples > 1)
	    score[i] -= (20 - format[i]->d.samples);

	if (format[i]->types & GLITZ_DRAWABLE_TYPE_WINDOW_MASK)
	    score[i] += 10;

	if (format[i]->types & GLITZ_DRAWABLE_TYPE_PBUFFER_MASK)
	    score[i] += 10;

	if (format[i]->caveat)
	    score[i] -= 1000;
    }

    return score[1] - score[0];
}

static void
_glitz_add_format (glitz_glucose_screen_info_t     *screen_info,
		   glitz_int_drawable_format_t *format)
{
    int n = screen_info->n_formats;

    screen_info->formats =
	realloc (screen_info->formats,
		 sizeof (glitz_int_drawable_format_t) * (n + 1));
    if (screen_info->formats)
    {
	screen_info->formats[n] = *format;
	screen_info->formats[n].d.id = n;
	screen_info->n_formats++;
    }
}

static void
_glitz_glucose_query_formats (glitz_glucose_screen_info_t *screen_info)
{
    __GLXscreen			*screen = screen_info->display_info->display;
    __GLcontextModes		*mode;
    glitz_int_drawable_format_t format;
    int				i;

    format.types          = GLITZ_DRAWABLE_TYPE_WINDOW_MASK;
    format.d.id           = 0;
    format.d.color.fourcc = GLITZ_FOURCC_RGB;

    mode = screen->modes;

    for (i = 0; i < screen->numVisuals; i++)
    {
	int value;

	if ((_gl_get_context_mode_data(mode, GLX_USE_GL, &value) != 0) ||
	    (value == 0))
	    continue;

	_gl_get_context_mode_data(mode, GLX_RGBA, &value);
	if (value == 0)
	    continue;

	/* Stereo is not supported yet */
	_gl_get_context_mode_data(mode, GLX_STEREO, &value);
	if (value != 0)
	    continue;

	_gl_get_context_mode_data(mode, GLX_RED_SIZE, &value);
	format.d.color.red_size = (unsigned short) value;
	_gl_get_context_mode_data(mode, GLX_GREEN_SIZE, &value);
	format.d.color.green_size = (unsigned short) value;
	_gl_get_context_mode_data(mode, GLX_BLUE_SIZE, &value);
	format.d.color.blue_size = (unsigned short) value;
	_gl_get_context_mode_data(mode, GLX_ALPHA_SIZE, &value);
	format.d.color.alpha_size = (unsigned short) value;
	_gl_get_context_mode_data(mode, GLX_DEPTH_SIZE, &value);
	format.d.depth_size = (unsigned short) value;
	_gl_get_context_mode_data(mode, GLX_STENCIL_SIZE, &value);
	format.d.stencil_size = (unsigned short) value;
	_gl_get_context_mode_data(mode, GLX_DOUBLEBUFFER, &value);
	format.d.doublebuffer = (value) ? 1: 0;

	_gl_get_context_mode_data(mode, GLX_VISUAL_CAVEAT_EXT, &value);
	switch (value) {
	case GLX_SLOW_VISUAL_EXT:
	case GLX_NON_CONFORMANT_VISUAL_EXT:
	    format.caveat = 1;
	    break;
	default:
	    format.caveat = 0;
	    break;
	}

	_gl_get_context_mode_data(mode, GLX_SAMPLE_BUFFERS_ARB, &value);
	if (value)
	{
	    _gl_get_context_mode_data(mode, GLX_SAMPLES_ARB, &value);
	    format.d.samples = (unsigned short) (value > 1)? value: 1;
	}
	else
	    format.d.samples = 1;

	format.u.uval = mode->visualID;

	_glitz_add_format (screen_info, &format);

	mode = mode->next;
    }
}

void
glitz_glucose_query_formats (glitz_glucose_screen_info_t *screen_info)
{
    int		   i;

    _glitz_glucose_query_formats (screen_info);

    if (!screen_info->n_formats)
	return;

    qsort (screen_info->formats, screen_info->n_formats,
	   sizeof (glitz_int_drawable_format_t), _glitz_glucose_format_compare);

    for (i = 0; i < screen_info->n_formats; i++)
	screen_info->formats[i].d.id = i;
}

glitz_drawable_format_t *
glitz_glucose_find_window_format (__GLXscreen *screen,
			      unsigned long                 mask,
			      const glitz_drawable_format_t *templ,
			      int                           count)
{
    glitz_int_drawable_format_t itempl;
    glitz_glucose_screen_info_t *screen_info =
	glitz_glucose_screen_info_get (screen);

    glitz_drawable_format_copy (templ, &itempl.d, mask);

    itempl.types = GLITZ_DRAWABLE_TYPE_WINDOW_MASK;
    mask |= GLITZ_INT_FORMAT_WINDOW_MASK;

    return glitz_drawable_format_find (screen_info->formats,
				       screen_info->n_formats,
				       mask, &itempl, count);
}
