#include <stdio.h>
#include <string.h>
#include <EGL/egl.h>

#ifdef __i386__
#include "libgdl.h"
#endif

#ifdef __mips__
#include <refsw/nexus_config.h>
#include <refsw/nexus_platform.h>
#include <refsw/nexus_display.h>
#include <refsw/nexus_core_utils.h>
#include <refsw/default_nexus.h>

extern unsigned int gs_screen_wdt;
extern unsigned int gs_screen_hgt;
extern void* gs_native_window;

#endif

static const char* const error_strings [] =
{
    "EGL_SUCCESS",
    "EGL_NOT_INITIALIZED",
    "EGL_BAD_ACCESS",
    "EGL_BAD_ALLOC",
    "EGL_BAD_ATTRIBUTE",
    "EGL_BAD_CONFIG",
    "EGL_BAD_CONTEXT",
    "EGL_BAD_CURRENT_SURFACE",
    "EGL_BAD_DISPLAY",
    "EGL_BAD_MATCH",
    "EGL_BAD_NATIVE_PIXMAP",
    "EGL_BAD_NATIVE_WINDOW",
    "EGL_BAD_PARAMETER",
    "EGL_BAD_SURFACE",
    "EGL_CONTEXT_LOST"
};

static void handle_egl_error(const char *name)
{
    EGLint error_code = eglGetError();
    printf("'%s' returned egl error '%s' (0x%x)\n",
        name, error_strings[error_code-EGL_SUCCESS], error_code);
}

/*
   Note: fbdev_window is already defined if we are using ARM's EGL/eglplatform.h
*/
#ifndef _FBDEV_WINDOW_H_
typedef struct fbdev_window
{
    unsigned short width;
    unsigned short height;
} fbdev_window;
#endif

static fbdev_window window;

#ifdef __i386__
void egl_init(EGLDisplay* pdisplay, EGLSurface* psurface, EGLContext* pcontext, int width, int height, gdl_plane_id_t plane)
#else
void egl_init(EGLDisplay* pdisplay, EGLSurface* psurface, EGLContext* pcontext, int width, int height)
#endif
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig configs[2];
    EGLBoolean eRetStatus;
    EGLint major, minor;
    EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLint config_count;
    EGLint cfg_attribs[] = {EGL_BUFFER_SIZE,    EGL_DONT_CARE,
                            EGL_DEPTH_SIZE,     16,
                            EGL_RED_SIZE,       8,
                            EGL_GREEN_SIZE,     8,
                            EGL_BLUE_SIZE,      8,
                            //EGL_SAMPLE_BUFFERS, 1,
                            //EGL_SAMPLES,        4,
                            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                            EGL_NONE};
    int i;

    display = eglGetDisplay((EGLNativeDisplayType)EGL_DEFAULT_DISPLAY);

    eRetStatus = eglInitialize(display, &major, &minor);
    if (eRetStatus != EGL_TRUE)
        handle_egl_error("eglInitialize");

    eRetStatus = eglGetConfigs(display, configs, 2, &config_count);
    if (eRetStatus != EGL_TRUE)
        handle_egl_error("eglGetConfigs");

    eRetStatus = eglChooseConfig(display, cfg_attribs, configs, 2, &config_count);
    if (eRetStatus != EGL_TRUE || !config_count)
        handle_egl_error("eglChooseConfig");

    if (strstr(eglQueryString(display, EGL_VENDOR), "ARM")) {
        window.width = width;
        window.height = height;
        surface = eglCreateWindowSurface(display, configs[0], (EGLNativeWindowType)&window, NULL);
    }
#ifdef __i386__
    else if (strstr(eglQueryString(display, EGL_VENDOR), "Intel")) {
        surface = eglCreateWindowSurface(display, configs[0], (NativeWindowType)plane, NULL);
    }
#endif
#ifdef __mips__
    else if (strstr(eglQueryString(display, EGL_VENDOR), "Broadcom")) {
        NXPL_NativeWindowInfo win_info;

        win_info.x        = 0; 
        win_info.y        = 0;
        win_info.width    = gs_screen_wdt;
        win_info.height   = gs_screen_hgt;
        win_info.stretch  = true;
        win_info.clientID = 0; //FIXME hardcoding

        gs_native_window = NXPL_CreateNativeWindow ( &win_info );

        surface = eglCreateWindowSurface(display, configs[0], gs_native_window, NULL);
    }
#endif
    else {
        surface = eglCreateWindowSurface(display, configs[0], 0, NULL);
    }

    if (surface == EGL_NO_SURFACE) {
        handle_egl_error("eglCreateWindowSurface");
    }

    eRetStatus = eglBindAPI(EGL_OPENGL_ES_API);
    if (eRetStatus != EGL_TRUE) {
        handle_egl_error("eglBindAPI");
    }

    context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        handle_egl_error("eglCreateContext");
    }

    eRetStatus = eglMakeCurrent(display, surface, surface, context);
    if (eRetStatus != EGL_TRUE)
        handle_egl_error("eglMakeCurrent");

    *pdisplay = display;
    *psurface = surface;
    *pcontext = context;
}

void egl_exit(EGLDisplay display, EGLSurface surface, EGLContext context)
{
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(display);
}

void egl_swap(EGLDisplay display, EGLSurface surface)
{
    eglSwapBuffers(display, surface);
}
