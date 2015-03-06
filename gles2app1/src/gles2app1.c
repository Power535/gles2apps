#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef __i386__
#include "libgdl.h"
#endif

#ifdef __mips__
#include <refsw/nexus_config.h>
#include <refsw/nexus_platform.h>
#include <refsw/nexus_display.h>
#include <refsw/nexus_core_utils.h>
#include <refsw/default_nexus.h>
#endif

#ifdef __arm__
#include <bcm_host.h>
#endif

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


#ifdef __arm__

static DISPMANX_DISPLAY_HANDLE_T dispman_display = 0;

static EGLNativeWindowType createDispmanxLayer(void)
{
    VC_RECT_T dst_rect;
    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = 1280;
    dst_rect.height = 720;

    VC_RECT_T src_rect;
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = 1280 << 16;
    src_rect.height = 720 << 16;

    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);

    VC_DISPMANX_ALPHA_T alpha;
    alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
    alpha.opacity = 0xFF;
    alpha.mask = 0;

    DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add(
            dispman_update, dispman_display, 1, &dst_rect, 0, &src_rect,
            DISPMANX_PROTECTION_NONE, &alpha, (DISPMANX_CLAMP_T *)NULL, (DISPMANX_TRANSFORM_T)0);

    vc_dispmanx_update_submit_sync(dispman_update);

    EGL_DISPMANX_WINDOW_T *eglWindow = malloc(sizeof(EGL_DISPMANX_WINDOW_T));
    eglWindow->element = dispman_element;
    eglWindow->width = 1280;
    eglWindow->height = 720;

    return eglWindow;
}

// these constants are not in any headers (yet)
#define ELEMENT_CHANGE_LAYER          (1<<0)
#define ELEMENT_CHANGE_OPACITY        (1<<1)
#define ELEMENT_CHANGE_DEST_RECT      (1<<2)
#define ELEMENT_CHANGE_SRC_RECT       (1<<3)
#define ELEMENT_CHANGE_MASK_RESOURCE  (1<<4)
#define ELEMENT_CHANGE_TRANSFORM      (1<<5)


static void destroyDispmanxLayer(EGLNativeWindowType window)
{
    EGL_DISPMANX_WINDOW_T *eglWindow = (EGL_DISPMANX_WINDOW_T *)(window);
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
    vc_dispmanx_element_remove(dispman_update, eglWindow->element);
    vc_dispmanx_update_submit_sync(dispman_update);
    free(eglWindow);
}

#endif

#ifdef __mips__

static unsigned int gs_screen_wdt = 1280;
static unsigned int gs_screen_hgt = 720;

static NEXUS_DisplayHandle  gs_nexus_display = 0;
static void* gs_native_window = 0;

static NXPL_PlatformHandle  nxpl_handle = 0;

#if NEXUS_NUM_HDMI_OUTPUTS && !NEXUS_DTV_PLATFORM

static void hotplug_callback(void *pParam, int iParam)
{
   NEXUS_HdmiOutputStatus status;
   NEXUS_HdmiOutputHandle hdmi = (NEXUS_HdmiOutputHandle)pParam;
   NEXUS_DisplayHandle display = (NEXUS_DisplayHandle)iParam;

   NEXUS_HdmiOutput_GetStatus(hdmi, &status);

   printf("HDMI hotplug event: %s\n", status.connected?"connected":"not connected");

   /* the app can choose to switch to the preferred format, but it's not required. */
   if (status.connected)
   {
      NEXUS_DisplaySettings displaySettings;
      NEXUS_Display_GetSettings(display, &displaySettings);

      printf("Switching to preferred format %d\n", status.preferredVideoFormat);

      displaySettings.format = status.preferredVideoFormat;
      NEXUS_Display_SetSettings(display, &displaySettings);
   }
}

#endif

void InitHDMIOutput(NEXUS_DisplayHandle display)
{

#if NEXUS_NUM_HDMI_OUTPUTS && !NEXUS_DTV_PLATFORM

   NEXUS_HdmiOutputSettings      hdmiSettings;
   NEXUS_PlatformConfiguration   platform_config;

   NEXUS_Platform_GetConfiguration(&platform_config);

   if (platform_config.outputs.hdmi[0])
   {
      NEXUS_Display_AddOutput(display, NEXUS_HdmiOutput_GetVideoConnector(platform_config.outputs.hdmi[0]));

      /* Install hotplug callback -- video only for now */
      NEXUS_HdmiOutput_GetSettings(platform_config.outputs.hdmi[0], &hdmiSettings);

      hdmiSettings.hotplugCallback.callback = hotplug_callback;
      hdmiSettings.hotplugCallback.context = platform_config.outputs.hdmi[0];
      hdmiSettings.hotplugCallback.param = (int)display;

      NEXUS_HdmiOutput_SetSettings(platform_config.outputs.hdmi[0], &hdmiSettings);

      /* Force a hotplug to switch to a supported format if necessary */
      hotplug_callback(platform_config.outputs.hdmi[0], (int)display);
   }

#else

   UNUSED(display);

#endif

}

bool InitPlatform ( void )
{
   bool succeeded = true;
   NEXUS_Error err;

   NEXUS_PlatformSettings platform_settings;

   /* Initialise the Nexus platform */
   NEXUS_Platform_GetDefaultSettings(&platform_settings);
   platform_settings.openFrontend = false;

   /* Initialise the Nexus platform */
   err = NEXUS_Platform_Init(&platform_settings);

   if (err)
   {
      printf("Err: NEXUS_Platform_Init() failed\n");
      succeeded = false;
   }
   else
   {
      NEXUS_DisplayHandle    display = NULL;
      NEXUS_DisplaySettings  display_settings;

      NEXUS_Display_GetDefaultSettings(&display_settings);

      display = NEXUS_Display_Open(0, &display_settings);

      if (display == NULL)
      {
         printf("Err: NEXUS_Display_Open() failed\n");
         succeeded = false;
      }
      else
      {
          NEXUS_VideoFormatInfo   video_format_info;
          NEXUS_GraphicsSettings  graphics_settings;
          NEXUS_Display_GetGraphicsSettings(display, &graphics_settings);

          graphics_settings.horizontalFilter = NEXUS_GraphicsFilterCoeffs_eBilinear;
          graphics_settings.verticalFilter = NEXUS_GraphicsFilterCoeffs_eBilinear;

          NEXUS_Display_SetGraphicsSettings(display, &graphics_settings);
 
          InitHDMIOutput(display);

          NEXUS_Display_GetSettings(display, &display_settings);
          NEXUS_VideoFormat_GetInfo(display_settings.format, &video_format_info);

          gs_nexus_display = display;
          gs_screen_wdt = video_format_info.width;
          gs_screen_hgt = video_format_info.height;

          printf("Screen width %d, Screen height %d\n", gs_screen_wdt, gs_screen_hgt);
      }
   }

   if (succeeded == true)
   {
       NXPL_RegisterNexusDisplayPlatform ( &nxpl_handle, gs_nexus_display );
   } 
    
   return succeeded;
}


void DeInitPlatform ( void )
{
    NXPL_DestroyNativeWindow(gs_native_window);

    if ( gs_nexus_display != 0 )
    {
    	NXPL_UnregisterNexusDisplayPlatform ( nxpl_handle );
        //NEXUS_SurfaceClient_Release ( gs_native_window );
    }
    NEXUS_Platform_Uninit ();
}


#endif

static fbdev_window window;

EGLDisplay          display;
EGLConfig           config;
EGLSurface          surface;
EGLContext          context;


#ifdef __i386__
static int  _eglInit(gdl_plane_id_t plane);
#else
static int  _eglInit(void);
#endif

static int  _eglExit(void);
static void _eglClearError(void);
static int  _eglTestError(const char* text);

// Plane size and position
#define ORIGIN_X 0
#define ORIGIN_Y 0
#define WIDTH 1280
#define HEIGHT 720
#define ASPECT ((GLfloat)WIDTH / (GLfloat)HEIGHT)

#ifdef __i386__
// Initializes a plane for the graphics to be rendered to
static gdl_ret_t setup_plane(gdl_plane_id_t plane)
{
    gdl_pixel_format_t pixelFormat = GDL_PF_ARGB_32;
    gdl_color_space_t colorSpace = GDL_COLOR_SPACE_RGB;
    gdl_rectangle_t srcRect;
    gdl_rectangle_t dstRect;
    gdl_ret_t rc = GDL_SUCCESS;

    dstRect.origin.x = ORIGIN_X;
    dstRect.origin.y = ORIGIN_Y;
    dstRect.width = WIDTH;
    dstRect.height = HEIGHT;

    srcRect.origin.x = 0;
    srcRect.origin.y = 0;
    srcRect.width = WIDTH;
    srcRect.height = HEIGHT;

    rc = gdl_plane_reset(plane);
    if (GDL_SUCCESS == rc)
    {
        rc = gdl_plane_config_begin(plane);
    }

    if (GDL_SUCCESS == rc)
    {
        rc = gdl_plane_set_attr(GDL_PLANE_SRC_COLOR_SPACE, &colorSpace);
    }

    if (GDL_SUCCESS == rc)
    {
        rc = gdl_plane_set_attr(GDL_PLANE_PIXEL_FORMAT, &pixelFormat);
    }

    if (GDL_SUCCESS == rc)
    {
        rc = gdl_plane_set_attr(GDL_PLANE_DST_RECT, &dstRect);
    }

    if (GDL_SUCCESS == rc)
    {
        rc = gdl_plane_set_attr(GDL_PLANE_SRC_RECT, &srcRect);
    }

    if (GDL_SUCCESS == rc)
    {
        rc = gdl_plane_config_end(GDL_FALSE);
    }
    else
    {
        gdl_plane_config_end(GDL_TRUE);
    }

    if (GDL_SUCCESS != rc)
    {
        fprintf(stderr,"GDL configuration failed! GDL error code is 0x%x\n", rc);
    }
  
    return rc;
}
#endif

#ifdef __i386__
static int _eglInit(gdl_plane_id_t plane)
#else
static int _eglInit()
#endif
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

                                        // EGL_SAMPLE_BUFFERS,     1,
                                        // EGL_SAMPLES,            4,

                                        EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,

                                        EGL_DEPTH_SIZE,         16,

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
        surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)&window, NULL);
        printf("Arm\n");
    }
#ifdef __i386__
     else if (strstr(eglQueryString(display, EGL_VENDOR), "Intel")) {
        printf("Intel\n");
        surface = eglCreateWindowSurface(display, config, (NativeWindowType)plane, NULL);
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

           surface = eglCreateWindowSurface(display, config, gs_native_window, NULL);
    }
#endif

#ifdef __arm__

     else if (strstr(eglQueryString(display, EGL_VENDOR), "Broadcom")) {

           surface = eglCreateWindowSurface(display, config, createDispmanxLayer(), NULL);
    }
#endif

    else {
        surface = eglCreateWindowSurface(display, config, 0, NULL);
        printf("Other\n");
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

#if 1
    printf("eglDestroyContext()\n");usleep(50 * 1000);
    eglDestroyContext(display, context);
    _eglTestError("eglDestroyContext()");
#endif

    printf("eglDestroySurface()\n");usleep(50 * 1000);
    eglDestroySurface(display, surface);
    _eglTestError("eglDestroySurface()");

#if 1
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
#ifdef __i386__
    gdl_plane_id_t plane = GDL_PLANE_ID_UPP_C;

    gdl_init(0);

    setup_plane(plane);

    _eglInit(plane);

#else

    #ifdef __mips__

    InitPlatform();

    #endif


    #ifdef __arm__

    bcm_host_init();

    dispman_display = vc_dispmanx_display_open(0/* LCD */);

    #endif

    _eglInit();
#endif

    eglSwapInterval(display, 1);

    glViewport(0, 0, 1280, 720);

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);


    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    printf("eglSwapBuffers\n");usleep(50 * 1000);
    eglSwapBuffers(display, surface);
    usleep(500 * 1000);

    _eglExit();

    usleep(100 * 1000);

#ifdef __i386__
    gdl_close();
#endif

#ifdef __mips__
	DeInitPlatform();
#endif


#ifdef __arm__
    // destroyDispmanxLayer(window);
#endif

    usleep(100 * 1000);

    return 0;
}
