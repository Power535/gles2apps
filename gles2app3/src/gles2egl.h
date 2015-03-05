
#include <EGL/egl.h>

#ifdef __i386__
#include "libgdl.h"
#endif


#ifdef __i386__
void egl_init(EGLDisplay* pdisplay, EGLSurface* psurface, EGLContext* pcontext, int width, int height, gdl_plane_id_t plane);
#else
void egl_init(EGLDisplay* pdisplay, EGLSurface* psurface, EGLContext* pcontext, int width, int height);
#endif
void egl_exit(EGLDisplay display, EGLSurface surface, EGLContext context);
void egl_swap(EGLDisplay display, EGLSurface surface);
