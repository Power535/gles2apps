#include <stdlib.h>
#include <linux/fb.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <pthread.h>

#if defined(__i386__)
#define IS_INTELCE
#elif defined(HAVE_REFSW_NEXUS_CONFIG_H)
#define IS_BCM_NEXUS
#elif defined(__arm__)
#define IS_RPI
#else
#error NO KNOWN TARGET!
#endif

#ifdef IS_INTELCE
#include <libgdl.h>
#endif

#ifdef IS_BCM_NEXUS
//#define IS_BCM_NEXUS_CLIENT
#define PLATFORMSERVER_CLIENT
#if defined(PLATFORMSERVER_CLIENT) || defined(IS_BCM_NEXUS_CLIENT)
#ifdef IS_BCM_NEXUS_CLIENT
#include <refsw/nxclient.h>
#endif
#include <refsw/nexus_config.h>
#include <refsw/default_nexus.h>
#include <refsw/nexus_platform_client.h>
#else
#include <refsw/default_nexus.h>
#include <refsw/nexus_config.h>
#include <refsw/nexus_core_utils.h>
#include <refsw/nexus_display.h>
#include <refsw/nexus_platform.h>
#endif
#endif

#ifdef IS_RPI
#include <bcm_host.h>
#endif

#ifdef IS_BCM_NEXUS

#if defined(PLATFORMSERVER_CLIENT) || defined(IS_BCM_NEXUS_CLIENT)

static NEXUS_DisplayHandle gs_nexus_display = 0;
// static NEXUS_SurfaceClient* gs_native_window = 0;
static void *gs_native_window = 0;
static NXPL_PlatformHandle nxpl_handle = 0;
#ifdef IS_BCM_NEXUS_CLIENT
static NxClient_AllocResults allocResults;
#endif
static unsigned int gs_screen_wdt = 1280;
static unsigned int gs_screen_hgt = 720;

bool InitPlatform(void) {
#ifdef IS_BCM_NEXUS_CLIENT
  NxClient_AllocSettings allocSettings;
  NxClient_JoinSettings joinSettings;

  NxClient_GetDefaultJoinSettings(&joinSettings);
  snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "%s", "qtbrowser");
  fprintf(stderr, "NxClient_Join...\n");
  if (NxClient_Join(&joinSettings)) {
    fprintf(stderr, "Err: NxClient_Join() failed");
    return false;
  }

  NxClient_GetDefaultAllocSettings(&allocSettings);
  allocSettings.surfaceClient = 1;
  if (NxClient_Alloc(&allocSettings, &allocResults)) {
    fprintf(stderr, "Err: NxClient_Alloc() failed");
    return false;
  }
#else
  NEXUS_Platform_Join();
#endif

  NXPL_RegisterNexusDisplayPlatform(&nxpl_handle, gs_nexus_display);
  return true;
}

void DeInitPlatform(void) {
  NXPL_UnregisterNexusDisplayPlatform(nxpl_handle);
#ifdef IS_BCM_NEXUS_CLIENT
  NEXUS_SurfaceClient_Release(gs_native_window);
  NxClient_Free(&allocResults);
  NxClient_Uninit();
#endif
}

#else

static unsigned int gs_screen_wdt = 1280;
static unsigned int gs_screen_hgt = 720;

static NEXUS_DisplayHandle gs_nexus_display = 0;
static void *gs_native_window = 0;

static NXPL_PlatformHandle nxpl_handle = 0;

#if NEXUS_NUM_HDMI_OUTPUTS && !NEXUS_DTV_PLATFORM

static void hotplug_callback(void *pParam, int iParam) {
  NEXUS_HdmiOutputStatus status;
  NEXUS_HdmiOutputHandle hdmi = (NEXUS_HdmiOutputHandle)pParam;
  NEXUS_DisplayHandle display = (NEXUS_DisplayHandle)iParam;

  NEXUS_HdmiOutput_GetStatus(hdmi, &status);

  printf("HDMI hotplug event: %s\n",
         status.connected ? "connected" : "not connected");

  /* the app can choose to switch to the preferred format, but it's not
   * required. */
  if (status.connected) {
    NEXUS_DisplaySettings displaySettings;
    NEXUS_Display_GetSettings(display, &displaySettings);

    if (!status.videoFormatSupported[displaySettings.format]) {
      fprintf(stderr,
              "\nCurrent format not supported by attached "
              "monitor. Switching to preferred format %d\n",
              status.preferredVideoFormat);
      displaySettings.format = status.preferredVideoFormat;
      NEXUS_Display_SetSettings(display, &displaySettings);
    }
  }
}

#endif

void InitHDMIOutput(NEXUS_DisplayHandle display) {

#if NEXUS_NUM_HDMI_OUTPUTS && !NEXUS_DTV_PLATFORM

  NEXUS_HdmiOutputSettings hdmiSettings;
  NEXUS_PlatformConfiguration platform_config;

  NEXUS_Platform_GetConfiguration(&platform_config);

  if (platform_config.outputs.hdmi[0]) {
    NEXUS_Display_AddOutput(display, NEXUS_HdmiOutput_GetVideoConnector(
                                         platform_config.outputs.hdmi[0]));
    /* Install hotplug callback -- video only for now */
    NEXUS_HdmiOutput_GetSettings(platform_config.outputs.hdmi[0],
                                 &hdmiSettings);
    hdmiSettings.hotplugCallback.callback = hotplug_callback;
    hdmiSettings.hotplugCallback.context = platform_config.outputs.hdmi[0];
    hdmiSettings.hotplugCallback.param = (int)display;
    NEXUS_HdmiOutput_SetSettings(platform_config.outputs.hdmi[0],
                                 &hdmiSettings);
    /* Force a hotplug to switch to a supported format if necessary */
    hotplug_callback(platform_config.outputs.hdmi[0], (int)display);
  }

#else
  UNUSED(display);

#endif
}

bool InitPlatform(void) {
  bool succeeded = true;
  NEXUS_Error err;

  NEXUS_PlatformSettings platform_settings;
  NEXUS_Platform_GetDefaultSettings(&platform_settings);
  platform_settings.openFrontend = false;

  err = NEXUS_Platform_Init(&platform_settings);
  if (err) {
    printf("Err: NEXUS_Platform_Init() failed\n");
    succeeded = false;
  } else {
    NEXUS_DisplayHandle display = NULL;
    NEXUS_DisplaySettings display_settings;

    NEXUS_Display_GetDefaultSettings(&display_settings);

    display_settings.format = NEXUS_VideoFormat_e720p;

    display = NEXUS_Display_Open(0, &display_settings);

    if (display == NULL) {
      printf("Err: NEXUS_Display_Open() failed\n");
      succeeded = false;
    } else {
      NEXUS_VideoFormatInfo video_format_info;
      NEXUS_GraphicsSettings graphics_settings;
      NEXUS_Display_GetGraphicsSettings(display, &graphics_settings);

      graphics_settings.horizontalFilter = NEXUS_GraphicsFilterCoeffs_eBilinear;
      graphics_settings.verticalFilter = NEXUS_GraphicsFilterCoeffs_eBilinear;

#if 0
      /* Disable blend with video plane */
      graphics_settings.sourceBlendFactor =
          NEXUS_CompositorBlendFactor_eOne;
      graphics_settings.destBlendFactor =
          NEXUS_CompositorBlendFactor_eZero;
#endif

      NEXUS_Display_SetGraphicsSettings(display, &graphics_settings);
      InitHDMIOutput(display);
      NEXUS_Display_GetSettings(display, &display_settings);
      NEXUS_VideoFormat_GetInfo(display_settings.format, &video_format_info);
      gs_nexus_display = display;
      gs_screen_wdt = video_format_info.width;
      gs_screen_hgt = video_format_info.height;
      printf("Screen width %d, Screen height %d\n", gs_screen_wdt,
             gs_screen_hgt);
    }
  }

  if (succeeded == true) {
    NXPL_RegisterNexusDisplayPlatform(&nxpl_handle, gs_nexus_display);
  }

  return succeeded;
}

void DeInitPlatform(void) {
  NXPL_DestroyNativeWindow(gs_native_window);

  if (gs_nexus_display != 0) {
    NXPL_UnregisterNexusDisplayPlatform(nxpl_handle);
    // NEXUS_SurfaceClient_Release(gs_native_window);
  }
  NEXUS_Platform_Uninit();
}
#endif
#endif

#ifdef IS_RPI

static DISPMANX_DISPLAY_HANDLE_T dispman_display = 0;

static EGLNativeWindowType createDispmanxLayer(void) {
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
      DISPMANX_PROTECTION_NONE, &alpha, (DISPMANX_CLAMP_T *)NULL,
      (DISPMANX_TRANSFORM_T)0);

  vc_dispmanx_update_submit_sync(dispman_update);

  EGL_DISPMANX_WINDOW_T *eglWindow = malloc(sizeof(EGL_DISPMANX_WINDOW_T));
  eglWindow->element = dispman_element;
  eglWindow->width = 1280;
  eglWindow->height = 720;

  return eglWindow;
}

// these constants are not in any headers (yet)
#define ELEMENT_CHANGE_LAYER (1 << 0)
#define ELEMENT_CHANGE_OPACITY (1 << 1)
#define ELEMENT_CHANGE_DEST_RECT (1 << 2)
#define ELEMENT_CHANGE_SRC_RECT (1 << 3)
#define ELEMENT_CHANGE_MASK_RESOURCE (1 << 4)
#define ELEMENT_CHANGE_TRANSFORM (1 << 5)

static void destroyDispmanxLayer(EGLNativeWindowType window) {
  EGL_DISPMANX_WINDOW_T *eglWindow = (EGL_DISPMANX_WINDOW_T *)(window);
  DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
  vc_dispmanx_element_remove(dispman_update, eglWindow->element);
  vc_dispmanx_update_submit_sync(dispman_update);
  free(eglWindow);
}

#endif

static PFNEGLCREATESYNCKHRPROC __eglCreateSyncKHR;
static PFNEGLDESTROYSYNCKHRPROC __eglDestroySyncKHR;
static PFNEGLCLIENTWAITSYNCKHRPROC __eglClientWaitSyncKHR;
static PFNEGLGETSYNCATTRIBKHRPROC __eglGetSyncAttribKHR;

#ifdef IS_INTELCE
static void egl_init(EGLDisplay *pdisplay, EGLSurface *psurface,
                     EGLContext *pcontext, int width, int height,
                     gdl_plane_id_t plane);
#else
static void egl_init(EGLDisplay *pdisplay, EGLSurface *psurface,
                     EGLContext *pcontext, int width, int height);
#endif

#define PI ((float)3.14159265358979323846)
#define DegreesToRadians (PI / (float)180.0)

#define EGL_CHECK(x)                                                         \
  x;                                                                         \
  {                                                                          \
    EGLint eglError = eglGetError();                                         \
    if (eglError != EGL_SUCCESS) {                                           \
      printf("eglGetError() = %i (0x%.8x) at %s:%i\n", (signed int)eglError, \
             (unsigned int)eglError, __FILE__, __LINE__);                    \
      exit(1);                                                               \
    }                                                                        \
  }
#define WIDTH 1280
#define HEIGHT 720
#define BPP 4

GLubyte texdata[WIDTH * HEIGHT * BPP];

static const char *const error_strings[] = {
    "EGL_SUCCESS",       "EGL_NOT_INITIALIZED",     "EGL_BAD_ACCESS",
    "EGL_BAD_ALLOC",     "EGL_BAD_ATTRIBUTE",       "EGL_BAD_CONFIG",
    "EGL_BAD_CONTEXT",   "EGL_BAD_CURRENT_SURFACE", "EGL_BAD_DISPLAY",
    "EGL_BAD_MATCH",     "EGL_BAD_NATIVE_PIXMAP",   "EGL_BAD_NATIVE_WINDOW",
    "EGL_BAD_PARAMETER", "EGL_BAD_SURFACE",         "EGL_CONTEXT_LOST"};

static void handle_egl_error(const char *name) {
  EGLint error_code = eglGetError();
  printf("'%s' returned egl error '%s' (0x%x)\n", name,
         error_strings[error_code - EGL_SUCCESS], error_code);
}

#ifdef IS_INTELCE

// Plane size and position
#define ORIGIN_X 0
#define ORIGIN_Y 0
#define WIDTH 1280
#define HEIGHT 720
#define ASPECT ((GLfloat)WIDTH / (GLfloat)HEIGHT)

// Initializes a plane for the graphics to be rendered to
static gdl_ret_t setup_plane(gdl_plane_id_t plane) {
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
  if (GDL_SUCCESS == rc) {
    rc = gdl_plane_config_begin(plane);
  }

  if (GDL_SUCCESS == rc) {
    rc = gdl_plane_set_attr(GDL_PLANE_SRC_COLOR_SPACE, &colorSpace);
  }

  if (GDL_SUCCESS == rc) {
    rc = gdl_plane_set_attr(GDL_PLANE_PIXEL_FORMAT, &pixelFormat);
  }

  if (GDL_SUCCESS == rc) {
    rc = gdl_plane_set_attr(GDL_PLANE_DST_RECT, &dstRect);
  }

  if (GDL_SUCCESS == rc) {
    rc = gdl_plane_set_attr(GDL_PLANE_SRC_RECT, &srcRect);
  }

  if (GDL_SUCCESS == rc) {
    rc = gdl_plane_config_end(GDL_FALSE);
  } else {
    gdl_plane_config_end(GDL_TRUE);
  }

  if (GDL_SUCCESS != rc) {
    fprintf(stderr, "GDL configuration failed! GDL error code is 0x%x\n", rc);
  }

  return rc;
}

#endif

#ifdef IS_INTELCE
void egl_init(EGLDisplay *pdisplay, EGLSurface *psurface, EGLContext *pcontext,
              int width, int height, gdl_plane_id_t plane)
#else
void egl_init(EGLDisplay *pdisplay, EGLSurface *psurface, EGLContext *pcontext,
              int width, int height)
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
  EGLint cfg_attribs[] = {EGL_BUFFER_SIZE, EGL_DONT_CARE, EGL_DEPTH_SIZE, 16,
                          EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                          EGL_ALPHA_SIZE, 8,
                          //EGL_SAMPLE_BUFFERS, 1,
                          //EGL_SAMPLES,        4,
                          EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
  int i;

  display = eglGetDisplay((EGLNativeDisplayType)EGL_DEFAULT_DISPLAY);

  eRetStatus = eglInitialize(display, &major, &minor);
  if (eRetStatus != EGL_TRUE) handle_egl_error("eglInitialize");

  eRetStatus = eglGetConfigs(display, configs, 2, &config_count);
  if (eRetStatus != EGL_TRUE) handle_egl_error("eglGetConfigs");

  eRetStatus = eglChooseConfig(display, cfg_attribs, configs, 2, &config_count);
  if (eRetStatus != EGL_TRUE || !config_count)
    handle_egl_error("eglChooseConfig");

#ifdef IS_INTELCE
  else if (strstr(eglQueryString(display, EGL_VENDOR), "Intel")) {
    surface = eglCreateWindowSurface(display, configs[0],
                                     (NativeWindowType)plane, NULL);
  }
#endif
#ifdef IS_BCM_NEXUS
  else if (strstr(eglQueryString(display, EGL_VENDOR), "Broadcom")) {
    NXPL_NativeWindowInfo win_info;

    win_info.x = 0;
    win_info.y = 0;
    win_info.width = gs_screen_wdt;
    win_info.height = gs_screen_hgt;
    win_info.stretch = true;
    win_info.clientID = 0;  // FIXME hardcoding

    gs_native_window = NXPL_CreateNativeWindow(&win_info);

    surface =
        eglCreateWindowSurface(display, configs[0], gs_native_window, NULL);
  }
#endif

#ifdef IS_RPI

  else if (strstr(eglQueryString(display, EGL_VENDOR), "Broadcom")) {

    surface = eglCreateWindowSurface(display, configs[0], createDispmanxLayer(),
                                     NULL);
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

  context =
      eglCreateContext(display, configs[0], EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT) {
    handle_egl_error("eglCreateContext");
  }

  eRetStatus = eglMakeCurrent(display, surface, surface, context);
  if (eRetStatus != EGL_TRUE) handle_egl_error("eglMakeCurrent");

  *pdisplay = display;
  *psurface = surface;
  *pcontext = context;

#if 1
  typedef EGLSyncKHR(EGLAPIENTRYP PFNEGLCREATESYNCKHRPROC)(
      EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYSYNCKHRPROC)(EGLDisplay dpy,
                                                            EGLSyncKHR sync);
  typedef EGLint(EGLAPIENTRYP PFNEGLCLIENTWAITSYNCKHRPROC)(
      EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);
  typedef EGLBoolean(EGLAPIENTRYP PFNEGLGETSYNCATTRIBKHRPROC)(
      EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint * value);

  __eglCreateSyncKHR =
      (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
  __eglDestroySyncKHR =
      (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
  __eglClientWaitSyncKHR =
      (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
  __eglGetSyncAttribKHR =
      (PFNEGLGETSYNCATTRIBKHRPROC)eglGetProcAddress("eglGetSyncAttribKHR");
#endif
}

void egl_exit(EGLDisplay display, EGLSurface surface, EGLContext context) {
  eglDestroyContext(display, context);
  eglDestroySurface(display, surface);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglTerminate(display);
}

void egl_swap(EGLDisplay display, EGLSurface surface) {
  eglSwapBuffers(display, surface);
}

void Identity(float pMatrix[4][4]) {
  pMatrix[0][0] = 1.0f;
  pMatrix[0][1] = 0.0f;
  pMatrix[0][2] = 0.0f;
  pMatrix[0][3] = 0.0f;

  pMatrix[1][0] = 0.0f;
  pMatrix[1][1] = 1.0f;
  pMatrix[1][2] = 0.0f;
  pMatrix[1][3] = 0.0f;

  pMatrix[2][0] = 0.0f;
  pMatrix[2][1] = 0.0f;
  pMatrix[2][2] = 1.0f;
  pMatrix[2][3] = 0.0f;

  pMatrix[3][0] = 0.0f;
  pMatrix[3][1] = 0.0f;
  pMatrix[3][2] = 0.0f;
  pMatrix[3][3] = 1.0f;
}

static float Normalize(float afVout[3], float afVin[3]) {
  float fLen;

  fLen = afVin[0] * afVin[0] + afVin[1] * afVin[1] + afVin[2] * afVin[2];

  if (fLen <= 0.0f) {
    afVout[0] = 0.0f;
    afVout[1] = 0.0f;
    afVout[2] = 0.0f;
    return fLen;
  }

  if (fLen == 1.0F) {
    afVout[0] = afVin[0];
    afVout[1] = afVin[1];
    afVout[2] = afVin[2];
    return fLen;
  } else {
    fLen = ((float)1.0) / (float)sqrt((double)fLen);

    afVout[0] = afVin[0] * fLen;
    afVout[1] = afVin[1] * fLen;
    afVout[2] = afVin[2] * fLen;
    return fLen;
  }
}

void MultiplyMatrix(float psRes[4][4], float psSrcA[4][4], float psSrcB[4][4]) {
  float fB00, fB01, fB02, fB03;
  float fB10, fB11, fB12, fB13;
  float fB20, fB21, fB22, fB23;
  float fB30, fB31, fB32, fB33;
  int i;

  fB00 = psSrcB[0][0];
  fB01 = psSrcB[0][1];
  fB02 = psSrcB[0][2];
  fB03 = psSrcB[0][3];
  fB10 = psSrcB[1][0];
  fB11 = psSrcB[1][1];
  fB12 = psSrcB[1][2];
  fB13 = psSrcB[1][3];
  fB20 = psSrcB[2][0];
  fB21 = psSrcB[2][1];
  fB22 = psSrcB[2][2];
  fB23 = psSrcB[2][3];
  fB30 = psSrcB[3][0];
  fB31 = psSrcB[3][1];
  fB32 = psSrcB[3][2];
  fB33 = psSrcB[3][3];

  for (i = 0; i < 4; i++) {
    psRes[i][0] = psSrcA[i][0] * fB00 + psSrcA[i][1] * fB10 +
                  psSrcA[i][2] * fB20 + psSrcA[i][3] * fB30;
    psRes[i][1] = psSrcA[i][0] * fB01 + psSrcA[i][1] * fB11 +
                  psSrcA[i][2] * fB21 + psSrcA[i][3] * fB31;
    psRes[i][2] = psSrcA[i][0] * fB02 + psSrcA[i][1] * fB12 +
                  psSrcA[i][2] * fB22 + psSrcA[i][3] * fB32;
    psRes[i][3] = psSrcA[i][0] * fB03 + psSrcA[i][1] * fB13 +
                  psSrcA[i][2] * fB23 + psSrcA[i][3] * fB33;
  }
}

void Perspective(float pMatrix[4][4], float fovy, float aspect, float zNear,
                 float zFar) {
  float sine, cotangent, deltaZ;
  float radians;
  float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

  radians = fovy / 2 * PI / 180;

  deltaZ = zFar - zNear;
  sine = (float)sin(radians);
  if ((deltaZ == 0) || (sine == 0) || (aspect == 0)) {
    return;
  }

  cotangent = (float)cos(radians) / sine;

  m[0][0] = cotangent / aspect;
  m[1][1] = cotangent;
  m[2][2] = -(zFar + zNear) / deltaZ;
  m[2][3] = -1;
  m[3][2] = -2 * zNear * zFar / deltaZ;
  m[3][3] = 0;

  MultiplyMatrix(pMatrix, m, pMatrix);
}

void Orthographic(float pMatrix[4][4], float left, float right, float bottom,
                  float top, float zNear, float zFar) {
  float deltaX, deltaY, deltaZ;
  float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

  deltaX = right - left;
  deltaY = top - bottom;
  deltaZ = zFar - zNear;

  if ((deltaZ == 0) || (deltaX == 0) || (deltaY == 0)) {
    return;
  }

  m[0][0] = 2 / deltaX;
  m[1][1] = 2 / deltaY;
  m[2][2] = -2 / deltaZ;

  m[3][0] = -(right + left) / (right - left);
  m[3][1] = -(top + bottom) / (top - bottom);
  m[3][2] = -(zFar + zNear) / (zFar - zNear);

  MultiplyMatrix(pMatrix, m, pMatrix);
}

void Frustum(float pMatrix[4][4], float left, float right, float bottom,
             float top, float zNear, float zFar) {
  float deltaX, deltaY, deltaZ;
  float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

  deltaX = right - left;
  deltaY = top - bottom;
  deltaZ = zFar - zNear;

  if ((deltaZ == 0) || (deltaX == 0) || (deltaY == 0)) {
    return;
  }

  m[0][0] = 2 * zNear / deltaX;
  m[1][1] = 2 * zNear / deltaY;
  m[2][0] = (left + right) / deltaX;
  m[2][1] = (top + bottom) / deltaY;
  m[2][2] = -(zFar + zNear) / deltaZ;
  m[2][3] = -1;
  m[3][2] = -2 * zNear * zFar / deltaZ;
  m[3][3] = 0;

  MultiplyMatrix(pMatrix, m, pMatrix);
}

void Scale(float pMatrix[4][4], float fX, float fY, float fZ) {
  float fM0, fM1, fM2, fM3;

  fM0 = fX * pMatrix[0][0];
  fM1 = fX * pMatrix[0][1];
  fM2 = fX * pMatrix[0][2];
  fM3 = fX * pMatrix[0][3];
  pMatrix[0][0] = fM0;
  pMatrix[0][1] = fM1;
  pMatrix[0][2] = fM2;
  pMatrix[0][3] = fM3;

  fM0 = fY * pMatrix[1][0];
  fM1 = fY * pMatrix[1][1];
  fM2 = fY * pMatrix[1][2];
  fM3 = fY * pMatrix[1][3];
  pMatrix[1][0] = fM0;
  pMatrix[1][1] = fM1;
  pMatrix[1][2] = fM2;
  pMatrix[1][3] = fM3;

  fM0 = fZ * pMatrix[2][0];
  fM1 = fZ * pMatrix[2][1];
  fM2 = fZ * pMatrix[2][2];
  fM3 = fZ * pMatrix[2][3];
  pMatrix[2][0] = fM0;
  pMatrix[2][1] = fM1;
  pMatrix[2][2] = fM2;
  pMatrix[2][3] = fM3;
}

void Translate(float pMatrix[4][4], float fX, float fY, float fZ) {
  float fM30, fM31, fM32, fM33;

  fM30 = fX * pMatrix[0][0] + fY * pMatrix[1][0] + fZ * pMatrix[2][0] +
         pMatrix[3][0];
  fM31 = fX * pMatrix[0][1] + fY * pMatrix[1][1] + fZ * pMatrix[2][1] +
         pMatrix[3][1];
  fM32 = fX * pMatrix[0][2] + fY * pMatrix[1][2] + fZ * pMatrix[2][2] +
         pMatrix[3][2];
  fM33 = fX * pMatrix[0][3] + fY * pMatrix[1][3] + fZ * pMatrix[2][3] +
         pMatrix[3][3];

  pMatrix[3][0] = fM30;
  pMatrix[3][1] = fM31;
  pMatrix[3][2] = fM32;
  pMatrix[3][3] = fM33;
}

void Rotate(float pMatrix[4][4], float fX, float fY, float fZ, float fAngle) {
  float fRadians, fSine, fCosine, fAB, fBC, fCA, fT;
  float afAv[4], afAxis[4];
  float afMatrix[4][4];

  afAv[0] = fX;
  afAv[1] = fY;
  afAv[2] = fZ;
  afAv[3] = 0;

  Normalize(afAxis, afAv);

  fRadians = fAngle * DegreesToRadians;
  fSine = (float)sin(fRadians);
  fCosine = (float)cos(fRadians);

  fAB = afAxis[0] * afAxis[1] * (1 - fCosine);
  fBC = afAxis[1] * afAxis[2] * (1 - fCosine);
  fCA = afAxis[2] * afAxis[0] * (1 - fCosine);

  Identity(afMatrix);

  fT = afAxis[0] * afAxis[0];
  afMatrix[0][0] = fT + fCosine * (1 - fT);
  afMatrix[2][1] = fBC - afAxis[0] * fSine;
  afMatrix[1][2] = fBC + afAxis[0] * fSine;

  fT = afAxis[1] * afAxis[1];
  afMatrix[1][1] = fT + fCosine * (1 - fT);
  afMatrix[2][0] = fCA + afAxis[1] * fSine;
  afMatrix[0][2] = fCA - afAxis[1] * fSine;

  fT = afAxis[2] * afAxis[2];
  afMatrix[2][2] = fT + fCosine * (1 - fT);
  afMatrix[1][0] = fAB - afAxis[2] * fSine;
  afMatrix[0][1] = fAB + afAxis[2] * fSine;

  MultiplyMatrix(pMatrix, afMatrix, pMatrix);
}

static EGLDisplay global_display;
static EGLSurface global_surface;
static EGLContext global_context;

static int hProgramHandle;
static int position_attriblocation;
static int inputtexcoord_attriblocation;
static int mvp_pos;
int framecount;

static GLfloat projection[4][4] = {
    {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
static GLfloat modelview[4][4] = {
    {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
static GLfloat mvp[4][4] = {
    {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

#define FLOAT_TO_FIXED(x) (long)((x)*65536.0f)
static const char *const vertShader =
    "\n\
attribute vec4 position;              \n\
attribute vec2 inputtexcoord;         \n\
varying vec2 texcoord;                \n\
uniform mat4 mvp;                     \n\
void main(void)                       \n\
{                                     \n\
  texcoord = inputtexcoord;           \n\
  gl_Position = mvp * position;       \n\
  gl_Position.z = gl_Position.w;      \n\
}                                     \n";

static const char *const fragShader =
    "\n\
varying highp vec2 texcoord;          \n\
uniform sampler2D basetexture;        \n\
void main(void)                       \n\
{                                     \n\
  gl_FragColor = texture2D(basetexture, texcoord);       \n\
}                                     \n";

#if 1

static GLfixed vertices[] = {

    FLOAT_TO_FIXED(0.5),  FLOAT_TO_FIXED(-0.5), FLOAT_TO_FIXED(-0.5),
    FLOAT_TO_FIXED(-0.5), FLOAT_TO_FIXED(-0.5), FLOAT_TO_FIXED(0.5),
    FLOAT_TO_FIXED(0.5),  FLOAT_TO_FIXED(0.5),

};

static GLfloat texcoord[] = {

    1.0, 1.0, 0.0, 1.0, 0.0,
    0.0, 1.0, 0.0

};
#endif
int create_program(const char *v, const char *f) {
  char pszInfoLog[1024];
  int nShaderStatus, nInfoLogLength;

  int vertshaderhandle = glCreateShader(GL_VERTEX_SHADER);
  int fragshaderhandle = glCreateShader(GL_FRAGMENT_SHADER);
  int programhandle = glCreateProgram();

  char *pszProgramString0 = (char *)v;
  int nProgramLength0 = strlen(v);
  glShaderSource(vertshaderhandle, 1, (const char **)&pszProgramString0,
                 &nProgramLength0);
  glCompileShader(vertshaderhandle);
  glGetShaderiv(vertshaderhandle, GL_COMPILE_STATUS, &nShaderStatus);
  if (nShaderStatus != GL_TRUE) {
    printf("Error: Failed to compile GLSL shader\n");
    glGetShaderInfoLog(vertshaderhandle, 1024, &nInfoLogLength, pszInfoLog);
    printf("%s\n", pszInfoLog);
  }
  glAttachShader(programhandle, vertshaderhandle);

  char *pszProgramString1 = (char *)f;
  int nProgramLength1 = strlen(f);
  glShaderSource(fragshaderhandle, 1, (const char **)&pszProgramString1,
                 &nProgramLength1);
  glCompileShader(fragshaderhandle);
  glGetShaderiv(fragshaderhandle, GL_COMPILE_STATUS, &nShaderStatus);
  if (nShaderStatus != GL_TRUE) {
    printf("Error: Failed to compile GLSL shader\n");
    glGetShaderInfoLog(fragshaderhandle, 1024, &nInfoLogLength, pszInfoLog);
    printf("%s\n", pszInfoLog);
  }
  glAttachShader(programhandle, fragshaderhandle);

  glLinkProgram(programhandle);
  glGetProgramiv(programhandle, GL_LINK_STATUS, &nShaderStatus);
  if (nShaderStatus != GL_TRUE) {
    printf("Error: Failed to link GLSL program\n");
    glGetProgramInfoLog(programhandle, 1024, &nInfoLogLength, pszInfoLog);
    printf("%s\n", pszInfoLog);
  }

  glValidateProgram(programhandle);
  glGetProgramiv(programhandle, GL_VALIDATE_STATUS, &nShaderStatus);
  if (nShaderStatus != GL_TRUE) {
    printf("Error: Failed to validate GLSL program\n");
    glGetProgramInfoLog(programhandle, 1024, &nInfoLogLength, pszInfoLog);
    printf("%s\n", pszInfoLog);
  }

  return programhandle;
}

//  triangle_fan
//  2 components per vertex attribute : x,y
//
//            _____
//           |\    |
//  3 ___ 4  | \   |
//   |\  |   |  \  |
//   | \ |   |   \ |
//  2|__\|1  |____\|
//
//    x   y
//    1  -1  1 (right bottom)
//   -1  -1  2 (left  bottom)
//   -1   1  3 (left  top   )
//    1   1  4 (right top   )
//

GLuint vboId;
GLuint texId;
GLuint texId2;
GLuint rboId;
GLuint fboId;

void egl_sync(EGLDisplay display);

static void init(EGLDisplay display, int argc, char **argv) {
  int e;

  hProgramHandle = create_program(vertShader, fragShader);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(hProgramHandle);
  position_attriblocation = glGetAttribLocation(hProgramHandle, "position");
  inputtexcoord_attriblocation =
      glGetAttribLocation(hProgramHandle, "inputtexcoord");

  mvp_pos = glGetUniformLocation(hProgramHandle, "mvp");

  GLubyte *lpTex = texdata;
  GLuint i, j;
  for (j = 0; j < HEIGHT; j++) {
    for (i = 0; i < WIDTH; i++) {
      if ((i ^ j) & 0x80) {
        lpTex[0] = lpTex[1] = lpTex[2] = 0x00;
        lpTex[3] = 0x00;
      } else if ((i ^ j) & 0x40) {
        lpTex[0] = lpTex[1] = lpTex[2] = 0xff;
        lpTex[3] = 0xdf;
      } else {
        lpTex[0] = lpTex[1] = 0x00;
        lpTex[2] = 0xff;
        lpTex[3] = 0xdf;
      }

      lpTex += 4;
    }
  }

  glGenTextures(1, &texId2);
  glBindTexture(GL_TEXTURE_2D, texId2);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, texdata);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

/*
 * create fbo
 */

#define TEXTURE_WIDTH 1280
#define TEXTURE_HEIGHT 720

  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  // create a renderbuffer object to store depth info
  glGenRenderbuffers(1, &rboId);
  glBindRenderbuffer(GL_RENDERBUFFER, rboId);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, TEXTURE_WIDTH,
                        TEXTURE_HEIGHT);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glGenFramebuffers(1, &fboId);
  glBindFramebuffer(GL_FRAMEBUFFER, fboId);

  // attach the texture to FBO color attachment point
  glFramebufferTexture2D(GL_FRAMEBUFFER,        // 1. fbo target: GL_FRAMEBUFFER
                         GL_COLOR_ATTACHMENT0,  // 2. attachment point
                         GL_TEXTURE_2D,         // 3. tex target: GL_TEXTURE_2D
                         texId,                 // 4. tex ID
                         0);                    // 5. mipmap level: 0(base)

  // attach the renderbuffer to depth attachment point
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,  // 1. fbo target: GL_FRAMEBUFFER
                            GL_DEPTH_ATTACHMENT,  // 2. attachment point
                            GL_RENDERBUFFER,  // 3. rbo target: GL_RENDERBUFFER
                            rboId);           // 4. rbo ID

  // check FBO status
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "fbo error 0x%08x\n", status);
  }

  // switch back to window-system-provided framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  e = glGetError();
  if (e != GL_NO_ERROR) {
    printf("GL ERROR = %x\n", e);
  }
}

pthread_mutex_t lock;
pthread_t tid1;

void *myThreadFun1(void *vargp) {
  EGLConfig configs[2];
  EGLBoolean eRetStatus;
  EGLint major, minor;
  EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLint config_count;
  EGLint cfg_attribs[] = {EGL_BUFFER_SIZE,
                          EGL_DONT_CARE,
                          EGL_DEPTH_SIZE,
                          16,
                          EGL_RED_SIZE,
                          8,
                          EGL_GREEN_SIZE,
                          8,
                          EGL_BLUE_SIZE,
                          8,
                          EGL_ALPHA_SIZE,
                          8,
                          //EGL_SAMPLE_BUFFERS, 1,
                          //EGL_SAMPLES,        4,
                          EGL_RENDERABLE_TYPE,
                          EGL_OPENGL_ES2_BIT,
                          EGL_NONE};

  eglGetConfigs(global_display, configs, 2, &config_count);
  eglChooseConfig(global_display, cfg_attribs, configs, 2, &config_count);

  EGLContext context = eglCreateContext(global_display, configs[0],
                                        global_context, context_attribs);
  if (context == EGL_NO_CONTEXT) {
    handle_egl_error("eglCreateContext");
  }

  if (!eglMakeCurrent(global_display, global_surface, global_surface, context))
    handle_egl_error("eglMakeCurrent");

  int prog = create_program(vertShader, fragShader);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  usleep(1 * 1000 * 1000);

  while (1) {
// printf("thread1\n");

#if 1
    pthread_mutex_lock(&lock);
    if (!eglMakeCurrent(global_display, global_surface, global_surface,
                        context))
      handle_egl_error("eglMakeCurrent");

    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);

    glBindTexture(GL_TEXTURE_2D, texId2);

    glEnableVertexAttribArray(position_attriblocation);
    glVertexAttribPointer(position_attriblocation, 2, GL_FIXED, 0, 0, vertices);

    glEnableVertexAttribArray(inputtexcoord_attriblocation);
    glVertexAttribPointer(inputtexcoord_attriblocation, 2, GL_FLOAT, 0, 0,
                          texcoord);

    Identity(projection);
    Perspective(projection, 90, 1, 0, 1);
    Identity(modelview);
    Translate(modelview, 0, 0, -0.5);

    Rotate(modelview, 0, 0, 1, framecount);
    MultiplyMatrix(mvp, modelview, projection);
    glUniformMatrix4fv(mvp_pos, 1, GL_FALSE, &mvp[0][0]);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    egl_sync(global_display);

    if (!eglMakeCurrent(global_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                        EGL_NO_CONTEXT))
      handle_egl_error("eglMakeCurrent");
    pthread_mutex_unlock(&lock);
#endif

    usleep(20 * 1000);
  }
  // return NULL;
}

static void render(void) {

#if 0
  if (!eglMakeCurrent(global_display, global_surface, global_surface,
                      global_context))
    handle_egl_error("eglMakeCurrent");

    /*
     * Render _to_ texture
     */
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

  glClearColor(0, 1, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(hProgramHandle);

  glBindTexture(GL_TEXTURE_2D, texId2);

  glEnableVertexAttribArray(position_attriblocation);
  glVertexAttribPointer(position_attriblocation, 2, GL_FIXED, 0, 0, vertices);

  glEnableVertexAttribArray(inputtexcoord_attriblocation);
  glVertexAttribPointer(inputtexcoord_attriblocation, 2, GL_FLOAT, 0, 0,
                        texcoord);

  Identity(projection);
  Perspective(projection, 90, 1, 0, 1);
  Identity(modelview);
  Translate(modelview, 0, 0, -0.5);

  Rotate(modelview, 0, 0, 1, framecount);
  MultiplyMatrix(mvp, modelview, projection);
  glUniformMatrix4fv(mvp_pos, 1, GL_FALSE, &mvp[0][0]);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
 egl_sync(global_display);
#endif

#if 1
  pthread_mutex_lock(&lock);
  if (!eglMakeCurrent(global_display, global_surface, global_surface,
                      global_context))
    handle_egl_error("eglMakeCurrent");
#endif

  // fprintf(stderr, "render\n");
  /*
   * Render _the_ texture
   */

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUseProgram(hProgramHandle);

  glClearColor(0, 1, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, texId);
  glEnableVertexAttribArray(position_attriblocation);
  glVertexAttribPointer(position_attriblocation, 2, GL_FIXED, 0, 0, vertices);

  glEnableVertexAttribArray(inputtexcoord_attriblocation);
  glVertexAttribPointer(inputtexcoord_attriblocation, 2, GL_FLOAT, 0, 0,
                        texcoord);

  Identity(projection);
  Perspective(projection, 90, 1, 0, 1);
  Identity(modelview);
  Translate(modelview, 0, 0, -0.5);
  Rotate(modelview, 0, 1, 0, framecount);
  MultiplyMatrix(mvp, modelview, projection);
  glUniformMatrix4fv(mvp_pos, 1, GL_FALSE, &mvp[0][0]);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  egl_sync(global_display);

  framecount++;

  egl_swap(global_display, global_surface);

#if 1
  if (!eglMakeCurrent(global_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      EGL_NO_CONTEXT))
    handle_egl_error("eglMakeCurrent");
  pthread_mutex_unlock(&lock);
  usleep(10 * 1000);

#endif
}

static EGLSyncKHR ESK = 0;

void egl_sync(EGLDisplay display) {
  if (!ESK) {
    ESK = __eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, 0);
    fprintf(stderr, "ESK=0x%08x\n", ESK);

    // glFlush is necessary to make sure that our fence creation reaches the GL
    // server before we try waiting on it from a different context
    glFlush();

    EGLint value;
    EGLBoolean ret;
    ret = __eglGetSyncAttribKHR(display, ESK, EGL_SYNC_TYPE_KHR, &value);
    fprintf(stderr, "EGL_SYNC_TYPE_KHR=0x%08x\n", value);
    ret = __eglGetSyncAttribKHR(display, ESK, EGL_SYNC_STATUS_KHR, &value);
    fprintf(stderr, "EGL_SYNC_STATUS_KHR=0x%08x\n", value);
    ret = __eglGetSyncAttribKHR(display, ESK, EGL_SYNC_CONDITION_KHR, &value);
    fprintf(stderr, "EGL_SYNC_CONDITION_KHR=0x%08x\n", value);
  }

  EGLint ret;
  do {
    ret = __eglClientWaitSyncKHR(display, ESK, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                                 500000000);
  } while (ret == EGL_TIMEOUT_EXPIRED_KHR);
}

int main(int argc, char **argv) {
  int i;

  struct fb_var_screeninfo varInfo;

#ifdef IS_INTELCE
  gdl_plane_id_t plane = GDL_PLANE_ID_UPP_C;
  gdl_init(0);
  setup_plane(plane);
  egl_init(&global_display, &global_surface, &global_context, varInfo.xres,
           varInfo.yres, plane);
#endif

#ifdef IS_BCM_NEXUS
  InitPlatform();
  egl_init(&global_display, &global_surface, &global_context, varInfo.xres,
           varInfo.yres);
#endif

#ifdef IS_RPI
  bcm_host_init();
  dispman_display = vc_dispmanx_display_open(0 /* LCD */);
  egl_init(&global_display, &global_surface, &global_context, varInfo.xres,
           varInfo.yres);
#endif

  egl_sync(global_display);

  init(global_display, argc, argv);

  pthread_mutex_init(&lock, NULL);
  pthread_create(&tid1, NULL, myThreadFun1, NULL);

  glClearColor(0, 0, 0, 0);
  while (1) {
    render();
  }

term:
  egl_exit(global_display, global_surface, global_context);

#ifdef IS_INTELCE
  gdl_close();
#endif

#ifdef IS_BCM_NEXUS
  DeInitPlatform();
#endif

#ifdef IS_RPI
// destroyDispmanxLayer(window);
#endif

  pthread_join(tid1, NULL);
  pthread_mutex_destroy(&lock);

  return 0;
}
