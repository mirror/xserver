/*
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

#ifndef GLITZ_GLUCOSE_H_INCLUDED
#define GLITZ_GLUCOSE_H_INCLUDED

#include "glitzint.h"

typedef glitz_function_pointer_t (* glitz_glucose_get_proc_address_t)
    (const glitz_gl_ubyte_t *);
typedef Bool (* glitz_glucose_make_context_current_t)
    (__GLXcontext *baseContext);
typedef __GLXcontext (* glitz_glucose_create_new_context_t)
    (__GLXscreen *display, int config, int render_type,
     __GLXcontext share_list, Bool direct);
typedef void *(* glitz_glucose_copy_sub_buffer_t)
    (__GLXscreen *display, __GLXdrawable *draw, int x, int y, int w, int h);

typedef struct _glitz_glucose_drawable glitz_glucose_drawable_t;
typedef struct _glitz_glucose_screen_info_t glitz_glucose_screen_info_t;
typedef struct _glitz_glucose_display_info_t glitz_glucose_display_info_t;

typedef struct _glitz_glucose_static_proc_address_list_t {
    glitz_glucose_get_proc_address_t         get_proc_address;
    glitz_glucose_make_context_current_t     make_context_current;
    glitz_glucose_create_new_context_t       create_new_context;
    glitz_glucose_copy_sub_buffer_t          copy_sub_buffer;
} glitz_glucose_static_proc_address_list_t;

typedef struct _glitz_glucose_thread_info_t {
    glitz_glucose_display_info_t **displays;
    int                      n_displays;
    glitz_context_t          *cctx;
} glitz_glucose_thread_info_t;

struct _glitz_glucose_display_info_t {
    glitz_glucose_thread_info_t *thread_info;
    __GLXscreen                 *display;
    glitz_glucose_screen_info_t **screens;
    int n_screens;
};

typedef struct _glitz_glucose_context_info_t {
    glitz_glucose_drawable_t *drawable;
    glitz_surface_t      *surface;
    glitz_constraint_t   constraint;
} glitz_glucose_context_info_t;

typedef struct _glitz_glucose_context_t {
    glitz_context_t   base;
    __GLXcontext      *context;
    glitz_format_id_t id;
    glitz_backend_t   backend;
    glitz_bool_t      initialized;
} glitz_glucose_context_t;

struct _glitz_glucose_screen_info_t {
    __GLXscreen				*screen;
    glitz_glucose_display_info_t             *display_info;
    int                                  drawables;
    glitz_int_drawable_format_t          *formats;
    int                                  n_formats;
    glitz_glucose_context_t                  **contexts;
    int                                  n_contexts;
    glitz_glucose_context_info_t           context_stack[GLITZ_CONTEXT_STACK_SIZE];
    int                                  context_stack_size;
    __GLXcontext                           *root_context;
    unsigned long                        glx_feature_mask;
    glitz_gl_float_t                     glx_version;
    glitz_glucose_static_proc_address_list_t glx;
    glitz_program_map_t                  program_map;
};

struct _glitz_glucose_drawable {
    glitz_drawable_t        base;

    glitz_glucose_screen_info_t *screen_info;
    glitz_glucose_context_t     *context;
    __GLXdrawable           *drawable;
    int                     width;
    int                     height;
};

extern void
glitz_glucose_query_extensions (glitz_glucose_screen_info_t *screen_info,
			    glitz_gl_float_t        glx_version);

extern glitz_glucose_screen_info_t *
glitz_glucose_screen_info_get (__GLXscreen *display);

extern glitz_function_pointer_t
glitz_glucose_get_proc_address (const char *name,
			    void       *closure);

extern glitz_glucose_context_t *
glitz_glucose_context_get (glitz_glucose_screen_info_t *screen_info,
		       glitz_drawable_format_t *format);

extern void
glitz_glucose_context_destroy (glitz_glucose_screen_info_t *screen_info,
			   glitz_glucose_context_t     *context);

extern void
glitz_glucose_query_formats (glitz_glucose_screen_info_t *screen_info);

extern glitz_bool_t
_glitz_glucose_drawable_update_size (glitz_glucose_drawable_t *drawable,
				 int                  width,
				 int                  height);

extern glitz_bool_t
glitz_glucose_push_current (void               *abstract_drawable,
			glitz_surface_t    *surface,
			glitz_constraint_t constraint,
			glitz_bool_t       *restore_state);

extern glitz_surface_t *
glitz_glucose_pop_current (void *abstract_drawable);

void
glitz_glucose_make_current (void               *abstract_drawable,
			glitz_constraint_t constraint);

extern glitz_status_t
glitz_glucose_make_current_read (void *abstract_surface);

extern void
glitz_glucose_destroy (void *abstract_drawable);

extern glitz_bool_t
glitz_glucose_swap_buffers (void *abstract_drawable);

extern glitz_bool_t
glitz_glucose_copy_sub_buffer (void *abstract_drawable,
			   int  x,
			   int  y,
			   int  width,
			   int  height);

glitz_drawable_format_t *
glitz_glucose_find_window_format (__GLXscreen              *screen,
			      unsigned long                 mask,
			      const glitz_drawable_format_t *templ,
			      int                           count);

glitz_drawable_t *
glitz_glucose_create_drawable_for_window (__GLXscreen *screen,
				      glitz_drawable_format_t *format,
				      __GLXdrawable           *window,
				      unsigned int            width,
				      unsigned int            height);

#endif /* GLITZ_GLUCOSE_H_INCLUDED */
