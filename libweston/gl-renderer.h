/*
 * Copyright © 2012 John Kåre Alsaker
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>

#include "compositor.h"

#ifdef ENABLE_EGL

#include <EGL/egl.h>
#include <EGL/eglext.h>

#else

typedef int EGLint;
typedef int EGLenum;
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef void *EGLConfig;
typedef intptr_t EGLNativeDisplayType;
typedef intptr_t EGLNativeWindowType;
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)

#endif /* ENABLE_EGL */

#define NO_EGL_PLATFORM 0
#define GL_RENDERER_API_NAME "gl_renderer_api"

enum gl_renderer_border_side {
	GL_RENDERER_BORDER_TOP = 0,
	GL_RENDERER_BORDER_LEFT = 1,
	GL_RENDERER_BORDER_RIGHT = 2,
	GL_RENDERER_BORDER_BOTTOM = 3,
};

enum gl_output_render_state {
	GL_RENDERER_DO_COMPOSITION = 0,
	GL_RENDERER_DO_NO_COMPOSITION,
	GL_RENDERER_DO_COMPOSITION_NO_SWAP,
	GL_RENDERER_OUTPUT_STATE_MAX
};

enum buffer_type {
	BUFFER_TYPE_NULL,
	BUFFER_TYPE_SOLID, /* internal solid color surfaces without a buffer */
	BUFFER_TYPE_SHM,
	BUFFER_TYPE_EGL,
	BUFFER_TYPE_DMA
};

struct egl_image_info
{
	int      buf_fd;
	int      buf_offset;
	int32_t  stride;
	int   	 gl_tex_target;
	int   	 gl_tex_format;
	EGLImageKHR image;
};

struct buffer_attrs
{
	int 	 width;
	int  	 height;
	int      y_inverted;
    uint32_t num_images;
	uint32_t dma_buf_format;
	EGLint   egl_buf_format;
	enum buffer_type  buf_type;
	struct egl_image_info egl_img[4];
};

struct gl_renderer_interface {
	const EGLint *opaque_attribs;
	const EGLint *alpha_attribs;

	int (*display_create)(struct weston_compositor *ec,
			      EGLenum platform,
			      void *native_window,
			      const EGLint *platform_attribs,
			      const EGLint *config_attribs,
			      const EGLint *visual_id,
			      const int n_ids);

	EGLDisplay (*display)(struct weston_compositor *ec);
	EGLContext (*context)(struct weston_compositor *ec);

	int (*output_window_create)(struct weston_output *output,
				    EGLNativeWindowType window_for_legacy,
				    void *window_for_platform,
				    const EGLint *config_attribs,
				    const EGLint *visual_id,
				    const int n_ids);

	void (*output_destroy)(struct weston_output *output);

	EGLSurface (*output_surface)(struct weston_output *output);

	/* Sets the output border.
	 *
	 * The side specifies the side for which we are setting the border.
	 * The width and height are the width and height of the border.
	 * The tex_width patemeter specifies the width of the actual
	 * texture; this may be larger than width if the data is not
	 * tightly packed.
	 *
	 * The top and bottom textures will extend over the sides to the
	 * full width of the bordered window.  The right and left edges,
	 * however, will extend only to the top and bottom of the
	 * compositor surface.  This is demonstrated by the picture below:
	 *
	 * +-----------------------+
	 * |          TOP          |
	 * +-+-------------------+-+
	 * | |                   | |
	 * |L|                   |R|
	 * |E|                   |I|
	 * |F|                   |G|
	 * |T|                   |H|
	 * | |                   |T|
	 * | |                   | |
	 * +-+-------------------+-+
	 * |        BOTTOM         |
	 * +-----------------------+
	 */
	void (*output_set_border)(struct weston_output *output,
				  enum gl_renderer_border_side side,
				  int32_t width, int32_t height,
				  int32_t tex_width, unsigned char *data);

	void (*print_egl_error_state)(void);

	void (*output_swap_buffer)(struct weston_output *output);

	int (*output_render_state)(struct weston_output *west_output,
			enum gl_output_render_state state);

	int (*ref_egl_image)(struct weston_surface *ec,
			struct weston_buffer *wbuf, struct buffer_attrs *buf_atr);

	int (*unref_egl_image)(struct weston_surface *ec,
			struct weston_buffer *wbuf, enum buffer_type buf_type);
};

