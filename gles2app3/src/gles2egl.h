
#include <EGL/egl.h>

void egl_init(EGLDisplay* pdisplay, EGLSurface* psurface, EGLContext* pcontext);
void egl_exit(EGLDisplay display, EGLSurface surface, EGLContext context);
void egl_swap(EGLDisplay display, EGLSurface surface);
