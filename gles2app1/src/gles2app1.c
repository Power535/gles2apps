#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>


#define EGLCHECK(x)                                         \
   do {                                                     \
        if (1)                                              \
        {                                                   \
            printf("[%s]\n", #x);                           \
        }                                                   \
                                                            \
        EGLint iErr = eglGetError();                        \
        if (iErr != EGL_SUCCESS) {                          \
            printf("%s failed (0x%08x).\n", text, iErr);    \
                                                            \
   } while (0)                                              \

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

EGLDisplay          display;
EGLConfig           config;
EGLSurface          surface;
EGLContext          context;


static int  _eglInit(void);
static int  _eglExit(void);

static void _eglClearError(void);
static int  _eglTestError(const char* text);


static int _eglInit(void)
{
    int      iConfigs;
    EGLint   WindowWidth, WindowHeight;
    EGLint   i32Values[5];

    EGLint   iMajorVersion, iMinorVersion;
    EGLint   ai32ContextAttribs[] =     {
                                        EGL_CONTEXT_CLIENT_VERSION, 2,

                                        EGL_NONE
                                        };

    EGLint   ai32ConfigAttribs[] =      {
                                        EGL_BUFFER_SIZE,        EGL_DONT_CARE,

                                        EGL_SAMPLE_BUFFERS,     1,
                                        EGL_SAMPLES,            4,

                                        EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,

                                        EGL_DEPTH_SIZE,         8,

                                        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,

                                        EGL_RED_SIZE,           8,
                                        EGL_GREEN_SIZE,         8,
                                        EGL_BLUE_SIZE,          8,

                                        EGL_NONE
                                        };

    printf("eglGetDisplay()\n");usleep(50 * 1000);
    display = eglGetDisplay((EGLNativeDisplayType)EGL_DEFAULT_DISPLAY);
    if (!_eglTestError("eglGetDisplay()")) return -1;

    printf("eglInitialize()\n");usleep(50 * 1000);
    eglInitialize(display, &iMajorVersion, &iMinorVersion);
    if (!_eglTestError("eglInitialize()")) return -1;

    printf("eglBindAPI()\n");usleep(50 * 1000);
    eglBindAPI(EGL_OPENGL_ES_API);
    if (!_eglTestError("eglBindAPI()")) return -1;

    printf("eglChooseConfig()\n");usleep(50 * 1000);
    eglChooseConfig(display, ai32ConfigAttribs, &config, 1, &iConfigs);
    if (!_eglTestError("eglChooseConfig()")) return -1;
    printf("iConfigs=%d\n", iConfigs);

    if (strstr(eglQueryString(display, EGL_VENDOR), "ARM")) {
        window.width = 1280;
        window.height = 720;
        surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType) &window, NULL);
    }
    else {
        surface = eglCreateWindowSurface(display, config, 0, NULL);
    }
    if (!_eglTestError("eglCreateWindowSurface()")) return -1;

    printf("eglCreateContext()\n");usleep(50 * 1000);
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, ai32ContextAttribs);
    if (!_eglTestError("eglCreateContext()")) return -1;

    printf("eglMakeCurrent()\n");usleep(50 * 1000);
    eglMakeCurrent(display, surface, surface, context);
    if (!_eglTestError("eglMakeCurrent()")) return -1;

    printf("eglQuerySurface()\n");usleep(50 * 1000);
    eglQuerySurface(display, surface, EGL_WIDTH, &WindowWidth);
    eglQuerySurface(display, surface, EGL_HEIGHT, &WindowHeight);
    printf("EGL WindowSize = %d x %d\n", WindowWidth, WindowHeight);

    printf("eglGetConfigAttrib()\n");usleep(50 * 1000);
    eglGetConfigAttrib(display, config, EGL_SAMPLE_BUFFERS , &i32Values[0]);
    eglGetConfigAttrib(display, config, EGL_SAMPLES , &i32Values[1]);
    printf("Sample buffers: %i\n", i32Values[0]);
    printf("Samples per pixel: %i\n", i32Values[1]);

    return 0;
}

static int _eglExit(void)
{
    printf("glFinish()\n");usleep(50 * 1000);
    glFinish();

    printf("eglWaitClient()\n");usleep(50 * 1000);
    eglWaitClient();

    printf("eglReleaseThread()\n");usleep(50 * 1000);
    eglReleaseThread();

    printf("eglMakeCurrent()\n");usleep(50 * 1000);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    _eglTestError("eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)...");

#if 0
    printf("eglDestroyContext()\n");usleep(50 * 1000);
    eglDestroyContext(display, context);
    _eglTestError("eglDestroyContext()");
#endif

    printf("eglDestroySurface()\n");usleep(50 * 1000);
    eglDestroySurface(display, surface);
    _eglTestError("eglDestroySurface()");

#if 0
    printf("eglTerminate()\n");usleep(50 * 1000);
    eglTerminate(display);
    _eglTestError("eglTerminate()");
#endif

    return 0;
}

static void _eglClearError(void)
{
    EGLint iErr = eglGetError();
}

static int _eglTestError(const char* text)
{
    EGLint iErr = eglGetError();
    if (iErr != EGL_SUCCESS) {
        printf("%s failed (0x%08x).\n", text, iErr);
        return 0;
    }

    return 1;
}


int main(int argc, char **argv)
{
    _eglInit();

    eglSwapInterval(display, 1);

    glViewport(0, 0, 1280, 720);

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);


    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    _eglExit();

    usleep(100 * 1000);
    return 0;
}
