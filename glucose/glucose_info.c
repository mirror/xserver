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
#include "glapi.h"
#include "glthread.h"
#include "dispatch.h"
#include "glitz_glucose.h"

#include <string.h>
#include <dlfcn.h>

#if DEBUG
#define glucoseFailure(n) \
	{ \
		int err = CALL_GetError( GET_DISPATCH(), () ); \
		if (err) \
			ErrorF("GL operation (" n ",%d) failed\n",err); \
	}
#else
#define glucoseFailure(n)
#endif

static void glucoseEnable( GLenum cap )
{
	CALL_Enable( GET_DISPATCH(), (cap) );
	glucoseFailure("Enable");
}

static void glucoseDisable( GLenum cap )
{
	CALL_Disable( GET_DISPATCH(), (cap) );
	glucoseFailure("Disable");
}

static GLenum glucoseGetError( void )
{
	return CALL_GetError( GET_DISPATCH(), () );
}

static const GLubyte * glucoseGetString( GLenum name )
{
	return CALL_GetString( GET_DISPATCH(), (name) );
}

static void glucoseEnableClientState( GLenum cap )
{
	CALL_EnableClientState( GET_DISPATCH(), (cap) );
	glucoseFailure("EnableClientState");
}

static void glucoseDisableClientState( GLenum cap )
{
	CALL_DisableClientState( GET_DISPATCH(), (cap) );
	glucoseFailure("DisableClientState");
}

static void
glucoseVertexPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	CALL_VertexPointer( GET_DISPATCH(), (size, type, stride, p) );
	glucoseFailure("VertexPointer");
}

static void
glucoseTexCoordPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	CALL_TexCoordPointer( GET_DISPATCH(), (size, type, stride, p) );
	glucoseFailure("TexCoordPointer");
}

static void
glucoseDrawArrays( GLenum mode, GLint first, GLsizei count )
{
	CALL_DrawArrays( GET_DISPATCH(), (mode, first, count) );
	glucoseFailure("DrawArrays");
}

static void
glucoseTexEnvf( GLenum target, GLenum pname, GLfloat param )
{
	CALL_TexEnvf( GET_DISPATCH(), (target, pname, param) );
	glucoseFailure("TexEnvf");
}

static void
glucoseTexEnvfv( GLenum target, GLenum pname, const GLfloat *params)
{
	CALL_TexEnvfv( GET_DISPATCH(), (target, pname, params) );
	glucoseFailure("TexEnvfv");
}

static void
glucoseTexGeni( GLenum coord, GLenum pname, GLint param )
{
	CALL_TexGeni( GET_DISPATCH(), (coord, pname, param) );
	glucoseFailure("TexGeni");
}

static void
glucoseTexGenfv( GLenum coord, GLenum pname, const GLfloat *params )
{
	CALL_TexGenfv( GET_DISPATCH(), (coord, pname, params) );
	glucoseFailure("TexGenfv");
}

static void
glucoseColor4us( GLushort red, GLushort green, GLushort blue, GLushort alpha )
{
	CALL_Color4us( GET_DISPATCH(), (red, green, blue, alpha) );
	glucoseFailure("Color4us");
}

static void
glucoseColor4f( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha )
{
	CALL_Color4f( GET_DISPATCH(), (red, green, blue, alpha) );
	glucoseFailure("Color4f");
}

static void
glucoseScissor( GLint x, GLint y, GLsizei width, GLsizei height)
{
	CALL_Scissor( GET_DISPATCH(), (x, y, width, height) );
	glucoseFailure("Scissor");
}

static void
glucoseBlendFunc( GLenum sfactor, GLenum dfactor )
{
	CALL_BlendFunc( GET_DISPATCH(), (sfactor, dfactor) );
	glucoseFailure("BlendFunc");
}

static void
glucoseClear( GLbitfield mask )
{
	CALL_Clear( GET_DISPATCH(), (mask) );
	glucoseFailure("Clear");
}

static void
glucoseClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
	CALL_ClearColor( GET_DISPATCH(), (red, green, blue, alpha) );
	glucoseFailure("ClearColor");
}

static void
glucoseClearStencil( GLint s )
{
	CALL_ClearStencil( GET_DISPATCH(), (s) );
	glucoseFailure("ClearStencil");
}

static void
glucoseStencilFunc( GLenum func, GLint ref, GLuint mask )
{
	CALL_StencilFunc( GET_DISPATCH(), (func, ref, mask) );
	glucoseFailure("StencilFunc");
}

static void
glucoseStencilOp( GLenum fail, GLenum zfail, GLenum zpass )
{
	CALL_StencilOp( GET_DISPATCH(), (fail, zfail, zpass) );
	glucoseFailure("StencilOp");
}

static void
glucosePushAttrib( GLbitfield mask )
{
	CALL_PushAttrib( GET_DISPATCH(), (mask) );
	glucoseFailure("PushAttrib");
}

static void
glucosePopAttrib( void )
{	
	CALL_PopAttrib( GET_DISPATCH(), () );
	glucoseFailure("PopAttrib");
}

static void
glucoseMatrixMode( GLenum mode )
{
	CALL_MatrixMode( GET_DISPATCH(), (mode) );
	glucoseFailure("MatrixMode");
}

static void
glucosePushMatrix( void )
{
	CALL_PushMatrix( GET_DISPATCH(), () );
	glucoseFailure("PushMatrix");
}

static void
glucosePopMatrix( void )
{
	CALL_PopMatrix( GET_DISPATCH(), () );
	glucoseFailure("PopMatrix");
}

static void
glucoseLoadIdentity( void )
{
	CALL_LoadIdentity( GET_DISPATCH(), () );
	glucoseFailure("LoadIdentity");
}

static void
glucoseLoadMatrixf( const GLfloat *m )
{
	CALL_LoadMatrixf( GET_DISPATCH(), (m) );
	glucoseFailure("LoadMatrixf");
}

static void
glucoseDepthRange( GLclampd near_val, GLclampd far_val )
{
	CALL_DepthRange( GET_DISPATCH(), (near_val, far_val) );
	glucoseFailure("DepthRange");
}

static void
glucoseViewport( GLint x, GLint y, GLsizei width, GLsizei height)
{
	CALL_Viewport( GET_DISPATCH(), (x, y, width, height) );
	glucoseFailure("Viewport");
}

static void
glucoseRasterPos2f( GLfloat x, GLfloat y )
{
	CALL_RasterPos2f( GET_DISPATCH(), (x, y) );
	glucoseFailure("RasterPos2f");
}

static void
glucoseBitmap( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap )
{
	CALL_Bitmap( GET_DISPATCH(), (width, height, xorig, yorig, xmove, ymove, bitmap) );
	glucoseFailure("Bitmap");
}

static void
glucoseReadBuffer( GLenum mode )
{
	CALL_ReadBuffer( GET_DISPATCH(), (mode) );
	glucoseFailure("ReadBuffer");
}

static void
glucoseDrawBuffer( GLenum mode )
{
	CALL_DrawBuffer( GET_DISPATCH(), (mode) );
	glucoseFailure("DrawBuffer");
}

static void
glucoseCopyPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type )
{
	CALL_CopyPixels( GET_DISPATCH(), (x, y, width, height, type) );
	glucoseFailure("CopyPixels");
}

static void
glucoseFlush( void )
{
	CALL_Flush( GET_DISPATCH(), () );
	glucoseFailure("Flush");
}

static void
glucoseFinish( void )
{
	CALL_Finish( GET_DISPATCH(), () );
	glucoseFailure("Finish");
}

static void
glucosePixelStorei( GLenum pname, GLint param )
{
	CALL_PixelStorei( GET_DISPATCH(), (pname, param) );
	glucoseFailure("PixelStorei");
}

static void
glucoseOrtho( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val)
{
	CALL_Ortho( GET_DISPATCH(), (left, right, bottom, top, near_val, far_val) );
	glucoseFailure("Ortho");
}

static void
glucoseScalef( GLfloat x, GLfloat y, GLfloat z )
{
	CALL_Scalef( GET_DISPATCH(), (x, y, z) );
	glucoseFailure("Scalef");
}

static void
glucoseTranslatef( GLfloat x, GLfloat y, GLfloat z )
{
	CALL_Translatef( GET_DISPATCH(), (x, y, z) );
	glucoseFailure("Translatef");
}

static void
glucoseHint( GLenum target, GLenum mode )
{
	CALL_Hint( GET_DISPATCH(), (target, mode) );
	glucoseFailure("Hint");
}

static void
glucoseDepthMask( GLboolean flag )
{
	CALL_DepthMask( GET_DISPATCH(), (flag) );
	glucoseFailure("DepthMask");
}

static void
glucosePolygonMode( GLenum face, GLenum mode )
{
	CALL_PolygonMode( GET_DISPATCH(), (face, mode) );
	glucoseFailure("PolygonMode");
}

static void
glucoseShadeModel( GLenum mode )
{
	CALL_ShadeModel( GET_DISPATCH(), (mode) );
	glucoseFailure("ShadeModel");
}

static void
glucoseColorMask( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha )
{
	CALL_ColorMask( GET_DISPATCH(), (red, green, blue, alpha) );
	glucoseFailure("ColorMask");
}

static void
glucoseReadPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels )
{
	CALL_ReadPixels( GET_DISPATCH(), (x, y, width, height, format, type, pixels) );
	glucoseFailure("ReadPixels");
}

static void
glucoseGetTexImage( GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels )
{
	CALL_GetTexImage( GET_DISPATCH(), (target, level, format, type, pixels) );
	glucoseFailure("GetTexImage");
}

static void
glucoseTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels )
{
	CALL_TexSubImage2D( GET_DISPATCH(), (target, level, xoffset, yoffset, width, height, format, type, pixels) );
	glucoseFailure("TexSubImage2D");
}

static void
glucoseGenTextures( GLsizei n, GLuint *textures )
{
	CALL_GenTextures( GET_DISPATCH(), (n, textures) );
	glucoseFailure("GenTextures");
}

static void
glucoseDeleteTextures( GLsizei n, const GLuint *textures)
{
	CALL_DeleteTextures( GET_DISPATCH(), (n, textures) );
	glucoseFailure("DeleteTextures");
}

static void
glucoseBindTexture( GLenum target, GLuint texture )
{
	CALL_BindTexture( GET_DISPATCH(), (target, texture) );
	glucoseFailure("BindTexture");
}

static void
glucoseTexImage2D( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels )
{
	CALL_TexImage2D( GET_DISPATCH(), (target, level, internalFormat, width, height, border, format, type, pixels) );
	glucoseFailure("TexImage2D");
}

static void
glucoseTexParameteri( GLenum target, GLenum pname, GLint param )
{
	CALL_TexParameteri( GET_DISPATCH(), (target, pname, param) );
	glucoseFailure("TexParameteri");
}

static void
glucoseTexParameterfv( GLenum target, GLenum pname, const GLfloat *params )
{
	CALL_TexParameterfv( GET_DISPATCH(), (target, pname, params) );
	glucoseFailure("TexParameterfv");
}

static void
glucoseGetTexLevelParameteriv( GLenum target, GLint level, GLenum pname, GLint *params )
{
	CALL_GetTexLevelParameteriv( GET_DISPATCH(), (target, level, pname, params) );
	glucoseFailure("GetTexLevelParameteriv");
}

static void
glucoseCopyTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height )
{
	CALL_CopyTexSubImage2D( GET_DISPATCH(), (target, level, xoffset, yoffset, x, y, width, height) );
	glucoseFailure("CopyTexSubImage2D");
}

static void
glucoseGetIntegerv( GLenum pname, GLint *params )
{
	CALL_GetIntegerv( GET_DISPATCH(), (pname, params) );
	glucoseFailure("GetIntegerv");
}

static void
glucoseBlendColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
	CALL_BlendColor( GET_DISPATCH(), (red, green, blue, alpha) );
	glucoseFailure("BlendColor");
}

static void
glucoseActiveTexture( GLenum texture )
{
	CALL_ActiveTextureARB( GET_DISPATCH(), (texture) );
	glucoseFailure("ActiveTexture");
}

static void
glucoseClientActiveTexture( GLenum texture )
{
	CALL_ClientActiveTextureARB( GET_DISPATCH(), (texture) );
	glucoseFailure("ClientActiveTexture");
}

static void
glucoseMultiDrawArrays( GLenum mode, GLint *first, GLsizei *count, GLsizei primcount )
{
	CALL_MultiDrawArraysEXT( GET_DISPATCH(), (mode, first, count, primcount) );
	glucoseFailure("MultiDrawArrays");
}

static void
glucoseGenPrograms( GLsizei n, GLuint *ids )
{
	CALL_GenProgramsNV( GET_DISPATCH(), (n, ids) );
}

static void
glucoseDeletePrograms( GLsizei n, const GLuint *ids )
{
	CALL_DeleteProgramsNV( GET_DISPATCH(), (n, ids) );
}

static void
glucoseProgramString( GLenum target, GLenum format, GLsizei len, const GLubyte *program )
{
	CALL_ProgramStringARB( GET_DISPATCH(), (target, format, len, program) );
}

static void
glucoseBindProgram( GLenum target, GLuint id )
{
	CALL_BindProgramNV( GET_DISPATCH(), (target, id) );
}

static void
glucoseProgramLocalParameter4fv( GLenum target, GLuint id, const GLfloat *params )
{
	CALL_ProgramLocalParameter4fvARB( GET_DISPATCH(), (target, id, params) );
}

static void
glucoseGetProgramiv( GLuint program, GLenum pname, GLint *params )
{
	CALL_GetProgramiv( GET_DISPATCH(), (program, pname, params) );
}

static void
glucoseGenBuffers( GLsizei n, GLuint *buffers )
{
	CALL_GenBuffersARB( GET_DISPATCH(), (n, buffers) );
}

static void
glucoseDeleteBuffers( GLsizei n, const GLuint *buffers )
{
	CALL_DeleteBuffersARB( GET_DISPATCH(), (n, buffers) );
}

static void
glucoseBindBuffer( GLenum target, GLuint buffer )
{
	CALL_BindBufferARB( GET_DISPATCH(), (target, buffer) );
	glucoseFailure("BindBuffer");
}

static void
glucoseBufferData( GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
	CALL_BufferDataARB( GET_DISPATCH(), (target, size, data, usage) );
}

static void
glucoseBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data )
{
	CALL_BufferSubDataARB( GET_DISPATCH(), (target, offset, size, data) );
}

static void
glucoseGetBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, GLvoid *data)
{
	CALL_GetBufferSubDataARB( GET_DISPATCH(), (target, offset, size, data) );
}

static void
glucoseMapBuffer(GLenum target, GLenum access)
{
	CALL_MapBufferARB( GET_DISPATCH(), (target, access) );
	glucoseFailure("MapBuffer");
}

static void
glucoseUnmapBuffer(GLenum target)
{
	CALL_UnmapBufferARB( GET_DISPATCH(), (target) );
	glucoseFailure("UnmapBuffer");
}

static void
glucoseGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	CALL_GenFramebuffersEXT( GET_DISPATCH(), (n, framebuffers) );
}

static void
glucoseDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	CALL_DeleteFramebuffersEXT( GET_DISPATCH(), (n, framebuffers) );
}

static void
glucoseBindFramebuffer(GLenum target, GLuint framebuffer)
{
	CALL_BindFramebufferEXT( GET_DISPATCH(), (target, framebuffer) );
}

static void
glucoseFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	CALL_FramebufferRenderbufferEXT( GET_DISPATCH(), (target, attachment, renderbuffertarget, renderbuffer) );
}

static void
glucoseFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	CALL_FramebufferTexture2DEXT( GET_DISPATCH(), (target, attachment, textarget, texture, level) );
}

static void
glucoseCheckFramebufferStatus(GLenum target)
{
	CALL_CheckFramebufferStatusEXT( GET_DISPATCH(), (target) );
}

static void
glucoseGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
	CALL_GenRenderbuffersEXT( GET_DISPATCH(), (n, renderbuffers) );
}

static void
glucoseDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
	CALL_DeleteRenderbuffersEXT( GET_DISPATCH(), (n, renderbuffers) );
}

static void
glucoseBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
	CALL_BindRenderbufferEXT( GET_DISPATCH(), (target, renderbuffer) );
}

static void
glucoseRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	CALL_RenderbufferStorageEXT( GET_DISPATCH(), (target, internalformat, width, height) );
}

static void
glucoseGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
	CALL_GetRenderbufferParameterivEXT( GET_DISPATCH(), (target, pname, params) );
}

glitz_gl_proc_address_list_t _glitz_glucose_gl_proc_address = {

    /* core */
    (glitz_gl_enable_t) glucoseEnable,
    (glitz_gl_disable_t) glucoseDisable,
    (glitz_gl_get_error_t) glucoseGetError,
    (glitz_gl_get_string_t) glucoseGetString,
    (glitz_gl_enable_client_state_t) glucoseEnableClientState,
    (glitz_gl_disable_client_state_t) glucoseDisableClientState,
    (glitz_gl_vertex_pointer_t) glucoseVertexPointer,
    (glitz_gl_tex_coord_pointer_t) glucoseTexCoordPointer,
    (glitz_gl_draw_arrays_t) glucoseDrawArrays,
    (glitz_gl_tex_env_f_t) glucoseTexEnvf,
    (glitz_gl_tex_env_fv_t) glucoseTexEnvfv,
    (glitz_gl_tex_gen_i_t) glucoseTexGeni,
    (glitz_gl_tex_gen_fv_t) glucoseTexGenfv,
    (glitz_gl_color_4us_t) glucoseColor4us,
    (glitz_gl_color_4f_t) glucoseColor4f,
    (glitz_gl_scissor_t) glucoseScissor,
    (glitz_gl_blend_func_t) glucoseBlendFunc,
    (glitz_gl_clear_t) glucoseClear,
    (glitz_gl_clear_color_t) glucoseClearColor,
    (glitz_gl_clear_stencil_t) glucoseClearStencil,
    (glitz_gl_stencil_func_t) glucoseStencilFunc,
    (glitz_gl_stencil_op_t) glucoseStencilOp,
    (glitz_gl_push_attrib_t) glucosePushAttrib,
    (glitz_gl_pop_attrib_t) glucosePopAttrib,
    (glitz_gl_matrix_mode_t) glucoseMatrixMode,
    (glitz_gl_push_matrix_t) glucosePushMatrix,
    (glitz_gl_pop_matrix_t) glucosePopMatrix,
    (glitz_gl_load_identity_t) glucoseLoadIdentity,
    (glitz_gl_load_matrix_f_t) glucoseLoadMatrixf,
    (glitz_gl_depth_range_t) glucoseDepthRange,
    (glitz_gl_viewport_t) glucoseViewport,
    (glitz_gl_raster_pos_2f_t) glucoseRasterPos2f,
    (glitz_gl_bitmap_t) glucoseBitmap,
    (glitz_gl_read_buffer_t) glucoseReadBuffer,
    (glitz_gl_draw_buffer_t) glucoseDrawBuffer,
    (glitz_gl_copy_pixels_t) glucoseCopyPixels,
    (glitz_gl_flush_t) glucoseFlush,
    (glitz_gl_finish_t) glucoseFinish,
    (glitz_gl_pixel_store_i_t) glucosePixelStorei,
    (glitz_gl_ortho_t) glucoseOrtho,
    (glitz_gl_scale_f_t) glucoseScalef,
    (glitz_gl_translate_f_t) glucoseTranslatef,
    (glitz_gl_hint_t) glucoseHint,
    (glitz_gl_depth_mask_t) glucoseDepthMask,
    (glitz_gl_polygon_mode_t) glucosePolygonMode,
    (glitz_gl_shade_model_t) glucoseShadeModel,
    (glitz_gl_color_mask_t) glucoseColorMask,
    (glitz_gl_read_pixels_t) glucoseReadPixels,
    (glitz_gl_get_tex_image_t) glucoseGetTexImage,
    (glitz_gl_tex_sub_image_2d_t) glucoseTexSubImage2D,
    (glitz_gl_gen_textures_t) glucoseGenTextures,
    (glitz_gl_delete_textures_t) glucoseDeleteTextures,
    (glitz_gl_bind_texture_t) glucoseBindTexture,
    (glitz_gl_tex_image_2d_t) glucoseTexImage2D,
    (glitz_gl_tex_parameter_i_t) glucoseTexParameteri,
    (glitz_gl_tex_parameter_fv_t) glucoseTexParameterfv,
    (glitz_gl_get_tex_level_parameter_iv_t) glucoseGetTexLevelParameteriv,
    (glitz_gl_copy_tex_sub_image_2d_t) glucoseCopyTexSubImage2D,
    (glitz_gl_get_integer_v_t) glucoseGetIntegerv,

    /* extensions */
    (glitz_gl_blend_color_t) 0,
    (glitz_gl_active_texture_t) 0,
    (glitz_gl_client_active_texture_t) 0,
    (glitz_gl_multi_draw_arrays_t) 0,
    (glitz_gl_gen_programs_t) 0,
    (glitz_gl_delete_programs_t) 0,
    (glitz_gl_program_string_t) 0,
    (glitz_gl_bind_program_t) 0,
    (glitz_gl_program_local_param_4fv_t) 0,
    (glitz_gl_get_program_iv_t) 0,
    (glitz_gl_gen_buffers_t) 0,
    (glitz_gl_delete_buffers_t) 0,
    (glitz_gl_bind_buffer_t) 0,
    (glitz_gl_buffer_data_t) 0,
    (glitz_gl_buffer_sub_data_t) 0,
    (glitz_gl_get_buffer_sub_data_t) 0,
    (glitz_gl_map_buffer_t) 0,
    (glitz_gl_unmap_buffer_t) 0,
    (glitz_gl_gen_framebuffers_t) 0,
    (glitz_gl_delete_framebuffers_t) 0,
    (glitz_gl_bind_framebuffer_t) 0,
    (glitz_gl_framebuffer_renderbuffer_t) 0,
    (glitz_gl_framebuffer_texture_2d_t) 0,
    (glitz_gl_check_framebuffer_status_t) 0,
    (glitz_gl_gen_renderbuffers_t) 0,
    (glitz_gl_delete_renderbuffers_t) 0,
    (glitz_gl_bind_renderbuffer_t) 0,
    (glitz_gl_renderbuffer_storage_t) 0,
    (glitz_gl_get_renderbuffer_parameter_iv_t) 0
};


glitz_function_pointer_t
glitz_glucose_get_proc_address (const char *name,
                            void       *closure)
{
    glitz_function_pointer_t address = NULL;
#if 0
    /* Unfortunately this doesn't work for us because glapi.c doesn't get
     * compiled with USE_X86_ASM, or appropriate, so never gets a generated
     * function pointer. Further investigation required.....
     */
    address = _glapi_get_proc_address(name);
#else

    if (!strncmp(name, "glBlendColor", 12))
    	address = (glitz_function_pointer_t) glucoseBlendColor;
    else if (!strncmp(name, "glActiveTexture", 15))
    	address = (glitz_function_pointer_t) glucoseActiveTexture;
    else if (!strncmp(name, "glClientActiveTexture", 21))
    	address = (glitz_function_pointer_t) glucoseClientActiveTexture;
    else if (!strncmp(name, "glMultiDrawArraysEXT", 20))
    	address = (glitz_function_pointer_t) glucoseMultiDrawArrays;
    else if (!strncmp(name, "glGenProgramsARB", 16))
    	address = (glitz_function_pointer_t) glucoseGenPrograms;
    else if (!strncmp(name, "glDeleteProgramsARB", 19))
    	address = (glitz_function_pointer_t) glucoseDeletePrograms;
    else if (!strncmp(name, "glProgramStringARB", 18))
    	address = (glitz_function_pointer_t) glucoseProgramString;
    else if (!strncmp(name, "glBindProgramARB", 16))
    	address = (glitz_function_pointer_t) glucoseBindProgram;
    else if (!strncmp(name, "glProgramLocalParameter4fvARB", 29))
    	address = (glitz_function_pointer_t) glucoseProgramLocalParameter4fv;
    else if (!strncmp(name, "glGetProgramivARB", 17))
    	address = (glitz_function_pointer_t) glucoseGetProgramiv;
    else if (!strncmp(name, "glGenBuffers", 12))
    	address = (glitz_function_pointer_t) glucoseGenBuffers;
    else if (!strncmp(name, "glDeleteBuffers", 15))
    	address = (glitz_function_pointer_t) glucoseDeleteBuffers;
    else if (!strncmp(name, "glBindBuffer", 12))
    	address = (glitz_function_pointer_t) glucoseBindBuffer;
    else if (!strncmp(name, "glBufferData", 12))
    	address = (glitz_function_pointer_t) glucoseBufferData;
    else if (!strncmp(name, "glBufferSubData", 15))
    	address = (glitz_function_pointer_t) glucoseBufferSubData;
    else if (!strncmp(name, "glGetBufferSubData", 18))
    	address = (glitz_function_pointer_t) glucoseGetBufferSubData;
    else if (!strncmp(name, "glMapBuffer", 11))
    	address = (glitz_function_pointer_t) glucoseMapBuffer;
    else if (!strncmp(name, "glUnmapBuffer", 13))
    	address = (glitz_function_pointer_t) glucoseUnmapBuffer;
    else if (!strncmp(name, "glGenFramebuffersEXT", 20))
    	address = (glitz_function_pointer_t) glucoseGenFramebuffers;
    else if (!strncmp(name, "glDeleteFramebuffersEXT", 23))
    	address = (glitz_function_pointer_t) glucoseDeleteFramebuffers;
    else if (!strncmp(name, "glBindFramebufferEXT", 20))
    	address = (glitz_function_pointer_t) glucoseBindFramebuffer;
    else if (!strncmp(name, "glFramebufferRenderbufferEXT", 28))
    	address = (glitz_function_pointer_t) glucoseFramebufferRenderbuffer;
    else if (!strncmp(name, "glFramebufferTexture2DEXT", 25))
    	address = (glitz_function_pointer_t) glucoseFramebufferTexture2D;
    else if (!strncmp(name, "glCheckFramebufferStatusEXT", 27))
    	address = (glitz_function_pointer_t) glucoseCheckFramebufferStatus;
    else if (!strncmp(name, "glGenRenderbuffersEXT", 21))
    	address = (glitz_function_pointer_t) glucoseGenRenderbuffers;
    else if (!strncmp(name, "glDeleteRenderbuffersEXT", 24))
    	address = (glitz_function_pointer_t) glucoseDeleteRenderbuffers;
    else if (!strncmp(name, "glBindRenderbufferEXT", 21))
    	address = (glitz_function_pointer_t) glucoseBindRenderbuffer;
    else if (!strncmp(name, "glRenderbufferStorageEXT", 24))
    	address = (glitz_function_pointer_t) glucoseRenderbufferStorage;
    else if (!strncmp(name, "glGetRenderbufferParameterivEXT", 31))
    	address = (glitz_function_pointer_t) glucoseGetRenderbufferParameteriv;
#endif

#if 0
    ErrorF("FUNCTION %s = %p\n",name,address);
#endif

    return address;
}

static void
_glitz_glucose_display_destroy (glitz_glucose_display_info_t *display_info);

static void
_glitz_glucose_screen_destroy (glitz_glucose_screen_info_t *screen_info);

static void
_glitz_glucose_thread_info_fini (glitz_glucose_thread_info_t *thread_info)
{
    int i;

    for (i = 0; i < thread_info->n_displays; i++)
	_glitz_glucose_display_destroy (thread_info->displays[i]);

    free (thread_info->displays);

    thread_info->displays = NULL;
    thread_info->n_displays = 0;

    thread_info->cctx = NULL;
}

static glitz_glucose_thread_info_t thread_info = {
    NULL,
    0,
    NULL
};

static glitz_glucose_thread_info_t *
_glitz_glucose_thread_info_get (const char *gl_library)
{
    return &thread_info;
}

static glitz_glucose_display_info_t *
_glitz_glucose_display_info_get (__GLXscreen *display)
{
    glitz_glucose_display_info_t *display_info;
    glitz_glucose_thread_info_t *thread_info = _glitz_glucose_thread_info_get (NULL);
    glitz_glucose_display_info_t **displays = thread_info->displays;
    int index, n_displays = thread_info->n_displays;

    for (; n_displays; n_displays--, displays++)
	if ((*displays)->display == display)
	    return *displays;

    index = thread_info->n_displays++;

    thread_info->displays =
	realloc (thread_info->displays,
		 sizeof (glitz_glucose_display_info_t *) *
		 thread_info->n_displays);

    display_info = malloc (sizeof (glitz_glucose_display_info_t));
    thread_info->displays[index] = display_info;

    display_info->thread_info = thread_info;
    display_info->display = display;
    display_info->screens = NULL;
    display_info->n_screens = 0;

    return display_info;
}

static void
_glitz_glucose_display_destroy (glitz_glucose_display_info_t *display_info)
{
    int i;

    for (i = 0; i < display_info->n_screens; i++)
	_glitz_glucose_screen_destroy (display_info->screens[i]);

    if (display_info->screens)
	free (display_info->screens);

    free (display_info);
}

glitz_glucose_screen_info_t *
glitz_glucose_screen_info_get (__GLXscreen *display)
{
    glitz_glucose_screen_info_t *screen_info;
    glitz_glucose_display_info_t *display_info =
	_glitz_glucose_display_info_get (display);
    glitz_glucose_screen_info_t **screens = display_info->screens;
    int index, n_screens = display_info->n_screens;

    for (; n_screens; n_screens--, screens++)
	if ((*screens)->screen == display)
	    return *screens;

    index = display_info->n_screens++;

    display_info->screens =
	realloc (display_info->screens,
		 sizeof (glitz_glucose_screen_info_t *) * display_info->n_screens);

    screen_info = malloc (sizeof (glitz_glucose_screen_info_t));
    display_info->screens[index] = screen_info;

    screen_info->display_info = display_info;
    screen_info->drawables = 0;
    screen_info->formats = NULL;
    screen_info->n_formats = 0;

    screen_info->contexts = NULL;
    screen_info->n_contexts = 0;

    glitz_program_map_init (&screen_info->program_map);

    screen_info->root_context = (__GLXcontext *)NULL;
    screen_info->glx_feature_mask = 0;

    glitz_glucose_query_formats (screen_info);

    screen_info->context_stack_size = 1;
    screen_info->context_stack->drawable = NULL;
    screen_info->context_stack->surface = NULL;
    screen_info->context_stack->constraint = GLITZ_NONE;

    return screen_info;
}

static void
_glitz_glucose_screen_destroy (glitz_glucose_screen_info_t *screen_info)
{
    int     i;

    if (screen_info->root_context)
	screen_info->root_context->makeCurrent (NULL);

    for (i = 0; i < screen_info->n_contexts; i++)
	glitz_glucose_context_destroy (screen_info, screen_info->contexts[i]);

    if (screen_info->contexts)
	free (screen_info->contexts);

    if (screen_info->formats)
	free (screen_info->formats);

    free (screen_info);
}
