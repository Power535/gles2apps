#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef MALI400

#define UMP

#define MALI_USE_DMA_BUF 0
#include <EGL/fbdev_window.h>
#include <ump/ump_ref_drv.h>

#else

typedef struct fbdev_window
{
    unsigned short width;
    unsigned short height;
} fbdev_window;

#endif

#define PI                  ((float) 3.14159265358979323846)
#define DegreesToRadians    (PI / (float) 180.0)

#define EGL_CHECK(x) \
    x; \
    { \
        EGLint eglError = eglGetError(); \
        if(eglError != EGL_SUCCESS) { \
            printf("eglGetError() = %i (0x%.8x) at %s:%i\n", (signed int)eglError, (unsigned int)eglError, __FILE__, __LINE__); \
            exit(1); \
        } \
    }

static fbdev_window window;

#define WIDTH   1280
#define HEIGHT  720
#define BPP     4

GLubyte texdata[WIDTH * HEIGHT * BPP];


#ifdef UMP

fbdev_pixmap *create_pixmap_ump(int width, int height, int red, int green, int blue, int alpha, int luminance, int ump, int yuv)
{
    int use_ump = ump;
    int size;
    fbdev_pixmap *pixmap;

    pixmap = malloc(sizeof(fbdev_pixmap));

    if (NULL == pixmap) {
        return NULL;
    }

    pixmap->flags = 0;
    pixmap->width = width;
    pixmap->height = height;
    pixmap->red_size = red;
    pixmap->green_size = green;
    pixmap->blue_size = blue;
    pixmap->alpha_size = alpha;
    pixmap->luminance_size = luminance;
    pixmap->buffer_size = (pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size);
    pixmap->bytes_per_pixel = pixmap->buffer_size / 8;
    pixmap->format = 0;
    size = pixmap->width * pixmap->height * pixmap->bytes_per_pixel;

    if (yuv) {
        pixmap->flags = 0;
        pixmap->width = width;
        pixmap->height = height;
        pixmap->red_size = 0;
        pixmap->green_size = 0;
        pixmap->blue_size = 0;
        pixmap->alpha_size = 0;
        pixmap->luminance_size = 8;
        pixmap->buffer_size = (pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size);
        pixmap->bytes_per_pixel = (pixmap->buffer_size + 7) / 8;
        pixmap->format = 0x30F4 /*EGL_YUV422I_KHR*/;
        size = pixmap->width * pixmap->height * 2;
    }

    if (1 == use_ump) {

        pixmap->flags = FBDEV_PIXMAP_SUPPORTS_UMP;

        #if 0
        pixmap->data = ump_handle_create_from_secure_id(secure_id);
        #else
        if (UMP_OK != ump_open()) {
            printf("%s: unable to open UMP interface\n", __func__);
            return NULL;
        }

        printf("%s: about to allocate YUV pixmap\n", __func__);
        pixmap->data = ump_ref_drv_allocate(size , UMP_REF_DRV_CONSTRAINT_NONE);
        if (UMP_INVALID_MEMORY_HANDLE == pixmap->data) {
            printf("%s: unable to allocate from UMP interface\n", __func__);
            ump_close();
            return NULL;
        }

        ump_write(pixmap->data, 0, texdata, size);
        printf("%s: create ump buffer\n", __func__);
        #endif

    } else {
        pixmap->data = malloc(size);
        if (NULL == pixmap->data) {
            free(pixmap);
            return NULL;
        }

        memcpy(pixmap->data, texdata, size);
    }

    return pixmap;
}

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

void egl_init(EGLDisplay* pdisplay, EGLSurface* psurface, EGLContext* pcontext, int width, int height)
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
                            EGL_SAMPLE_BUFFERS, 1,
                            EGL_SAMPLES,        4,
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
        surface = eglCreateWindowSurface(display, configs[0], (EGLNativeWindowType) &window, NULL);
    }
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

void Identity(float pMatrix[4][4])
{
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

static float Normalize(float afVout[3], float afVin[3])
{
    float fLen;

    fLen = afVin[0] * afVin[0] + afVin[1] * afVin[1] + afVin[2] * afVin[2];

    if (fLen <= 0.0f)
    {
        afVout[0] = 0.0f;
        afVout[1] = 0.0f;
        afVout[2] = 0.0f;
        return fLen;
    }

    if (fLen == 1.0F)
    {
        afVout[0] = afVin[0];
        afVout[1] = afVin[1];
        afVout[2] = afVin[2];
        return fLen;
    }
    else
    {
        fLen = ((float) 1.0) / (float)sqrt((double)fLen);

        afVout[0] = afVin[0] * fLen;
        afVout[1] = afVin[1] * fLen;
        afVout[2] = afVin[2] * fLen;
        return fLen;
    }
}

void MultiplyMatrix(float psRes[4][4], float psSrcA[4][4], float psSrcB[4][4])
{
    float fB00, fB01, fB02, fB03;
    float fB10, fB11, fB12, fB13;
    float fB20, fB21, fB22, fB23;
    float fB30, fB31, fB32, fB33;
    int i;

    fB00 = psSrcB[0][0]; fB01 = psSrcB[0][1];
    fB02 = psSrcB[0][2]; fB03 = psSrcB[0][3];
    fB10 = psSrcB[1][0]; fB11 = psSrcB[1][1];
    fB12 = psSrcB[1][2]; fB13 = psSrcB[1][3];
    fB20 = psSrcB[2][0]; fB21 = psSrcB[2][1];
    fB22 = psSrcB[2][2]; fB23 = psSrcB[2][3];
    fB30 = psSrcB[3][0]; fB31 = psSrcB[3][1];
    fB32 = psSrcB[3][2]; fB33 = psSrcB[3][3];

    for (i = 0; i < 4; i++)
    {
        psRes[i][0] = psSrcA[i][0] * fB00 + psSrcA[i][1] * fB10 + psSrcA[i][2] * fB20 + psSrcA[i][3] * fB30;
        psRes[i][1] = psSrcA[i][0] * fB01 + psSrcA[i][1] * fB11 + psSrcA[i][2] * fB21 + psSrcA[i][3] * fB31;
        psRes[i][2] = psSrcA[i][0] * fB02 + psSrcA[i][1] * fB12 + psSrcA[i][2] * fB22 + psSrcA[i][3] * fB32;
        psRes[i][3] = psSrcA[i][0] * fB03 + psSrcA[i][1] * fB13 + psSrcA[i][2] * fB23 + psSrcA[i][3] * fB33;
    }
}

void Perspective(float pMatrix[4][4], float fovy, float aspect, float zNear, float zFar)
{
    float sine, cotangent, deltaZ;
    float radians;
    float m[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};

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

void Orthographic(float pMatrix[4][4], float left, float right, float bottom, float top, float zNear, float zFar)
{
    float deltaX, deltaY, deltaZ;
    float m[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};

    deltaX = right - left;
    deltaY = top - bottom;
    deltaZ = zFar - zNear;

    if ((deltaZ == 0) || (deltaX == 0) || (deltaY == 0)) {
        return;
    }

    m[0][0] = 2 / deltaX;
    m[1][1] = 2 / deltaY;
    m[2][2] = -2 / deltaZ;

    m[3][0] = - ( right + left ) / ( right - left ) ;
    m[3][1] = - ( top + bottom ) / ( top - bottom ) ;
    m[3][2] = - ( zFar + zNear ) / ( zFar - zNear ) ;

    MultiplyMatrix(pMatrix, m, pMatrix);
}

void Frustum(float pMatrix[4][4], float left, float right, float bottom, float top, float zNear, float zFar)
{
    float deltaX, deltaY, deltaZ;
    float m[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};

    deltaX = right - left;
    deltaY = top - bottom;
    deltaZ = zFar - zNear;

    if ((deltaZ == 0) || (deltaX == 0) || (deltaY == 0)) {
        return;
    }

    m[0][0] = 2 * zNear / deltaX;
    m[1][1] = 2 * zNear / deltaY;
    m[2][0] = (left + right)/ deltaX;
    m[2][1] = (top + bottom)/ deltaY;
    m[2][2] = -(zFar + zNear) / deltaZ;
    m[2][3] = -1;
    m[3][2] = -2 * zNear * zFar / deltaZ;
    m[3][3] = 0;

    MultiplyMatrix(pMatrix, m, pMatrix);
}

void Scale(float pMatrix[4][4], float fX, float fY, float fZ)
{
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

void Translate(float pMatrix[4][4], float fX, float fY, float fZ)
{
    float fM30, fM31, fM32, fM33;

    fM30 = fX * pMatrix[0][0] + fY * pMatrix[1][0] + fZ * pMatrix[2][0] + pMatrix[3][0];
    fM31 = fX * pMatrix[0][1] + fY * pMatrix[1][1] + fZ * pMatrix[2][1] + pMatrix[3][1];
    fM32 = fX * pMatrix[0][2] + fY * pMatrix[1][2] + fZ * pMatrix[2][2] + pMatrix[3][2];
    fM33 = fX * pMatrix[0][3] + fY * pMatrix[1][3] + fZ * pMatrix[2][3] + pMatrix[3][3];

    pMatrix[3][0] = fM30;
    pMatrix[3][1] = fM31;
    pMatrix[3][2] = fM32;
    pMatrix[3][3] = fM33;
}

void Rotate(float pMatrix[4][4], float fX, float fY, float fZ, float fAngle)
{
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


static int frameStop = 0;

static int mvp_pos;
static int hProgramHandle;
static int attriblocation;

static GLfloat projection[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
static GLfloat modelview[4][4]  = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
static GLfloat mvp[4][4]        = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};

#define FLOAT_TO_FIXED(x) (long)((x) * 65536.0f)

static void signal_handler(int sig_num)
{
   if (sig_num == SIGINT || sig_num == SIGTERM || sig_num == SIGHUP) {
       frameStop = 1;
   }
}


static const char* const vertShader =                          "\n\
varying vec2 texcoord;                                          \n\
                                                                \n\
attribute vec4 position;                                        \n\
attribute vec2 inputtexcoord;                                   \n\
                                                                \n\
uniform mat4 mvp;                                               \n\
                                                                \n\
void main(void)                                                 \n\
{                                                               \n\
    texcoord = inputtexcoord;                                   \n\
                                                                \n\
    gl_Position = mvp * position;                               \n\
                                                                \n\
    gl_Position.z = gl_Position.w;                              \n\
}                                                               \n";


static const char* const fragShader =                          "\n\
varying highp vec2 texcoord;                                    \n\
                                                                \n\
uniform sampler2D basetexture;                                  \n\
                                                                \n\
void main(void)                                                 \n\
{                                                               \n\
    gl_FragColor = texture2D(basetexture, texcoord);            \n\
}                                                               \n";


static const char* const fragShader_uyvy =                     "\n\
precision mediump float;                                        \n\
varying highp vec2 texcoord;                                    \n\
                                                                \n\
uniform sampler2D basetexture;                                  \n\
                                                                \n\
void main(void)                                                 \n\
{                                                               \n\
    float sub = 0.0;                                            \n\
    if (gl_FragCoord.x > 512.0) sub = 512.5;                    \n\
                                                                \n\
    vec4 UYVY = texture2D(basetexture, texcoord);               \n\
                                                                \n\
    float xodd = mod(floor(gl_FragCoord.x - sub), 2.0);         \n\
    float Y;                                                    \n\
    float Cb = UYVY.z;                                          \n\
    float Cr = UYVY.x;                                          \n\
                                                                \n\
    Y = (UYVY.w * xodd) + (UYVY.y * (1.0 - xodd));              \n\
                                                                \n\
    gl_FragColor.rgb = Y * 1.164 + vec3( -0.87075, 0.53825, -1.08 ) + Cr * vec3( 1.596, -0.398, 0.0 ) + Cb * vec3( 0.0, -0.831, 2.018 );  \n\
                                                                \n\
    gl_FragColor.a = 1.0;                                       \n\
}                                                               \n";


static const char* const fragShader_oes =                      "\n\
#extension GL_OES_EGL_image_external : require                  \n\
precision mediump float;                                        \n\
varying vec2 texcoord;                                          \n\
                                                                \n\
uniform samplerExternalOES basetexture;                         \n\
                                                                \n\
void main(void)                                                 \n\
{                                                               \n\
    gl_FragColor = texture2D(basetexture, texcoord);            \n\
}                                                               \n";


static int init(EGLDisplay display, int argc, char **argv)
{
    // TRIANGLE
    #if 0
    static GLfixed vertices[] = {

        FLOAT_TO_FIXED(-0.5),   FLOAT_TO_FIXED(-0.5),
        FLOAT_TO_FIXED( 0),     FLOAT_TO_FIXED( 0.5),
        FLOAT_TO_FIXED( 0.5),   FLOAT_TO_FIXED(-0.5),
    };

    static GLfloat texcoord[] = {

        0.0, 0.0,
        1.0, 0.0,
        1.0, 1.0
    };
    #endif


    // TRIANGLE STRIP
    #if 0
    static GLfixed vertices[] = {

        FLOAT_TO_FIXED(-0.5),   FLOAT_TO_FIXED(-0.5),
        FLOAT_TO_FIXED( 0.5),   FLOAT_TO_FIXED(-0.5),
        FLOAT_TO_FIXED(-0.5),   FLOAT_TO_FIXED( 0.5),
        FLOAT_TO_FIXED( 0.5),   FLOAT_TO_FIXED( 0.5),
    };

    static GLfloat texcoord[] = {

        0.0, 1.0,
        1.0, 1.0,
        0.0, 0.0,
        1.0, 0.0
    };
    #endif


    //TRIANGLE FAN
    #if 1

    static GLfixed vertices[] = {

        FLOAT_TO_FIXED( 0.5),   FLOAT_TO_FIXED(-0.5),
        FLOAT_TO_FIXED(-0.5),   FLOAT_TO_FIXED(-0.5),
        FLOAT_TO_FIXED(-0.5),   FLOAT_TO_FIXED( 0.5),
        FLOAT_TO_FIXED( 0.5),   FLOAT_TO_FIXED( 0.5),

    };

    static GLfloat texcoord[] = {

      1.0, 1.0,
      0.0, 1.0,
      0.0, 0.0,
      1.0, 0.0

    };
    #endif


    GLubyte *lpTex = texdata;
    GLuint  i,j;
    char    pszInfoLog[1024];
    int     nShaderStatus, nInfoLogLength;
    int     hShaderHandle[3];

    EGLNativePixmapType pixmap;
    GLuint tex_id;
    EGLImageKHR egl_image;

    for (j = 0; j < HEIGHT; j++) {
        for (i = 0; i < WIDTH; i++) {
            if ((i ^ j) & 0x40) {
                lpTex[0] = lpTex[1] = lpTex[2] = 0x00; lpTex[3] = 0xff;
            } else {
                lpTex[0] = lpTex[1] = lpTex[2] = lpTex[3] = 0xFF;
            }
            lpTex += 4;
        }
    }

    if (argc > 1) {

        FILE *fs;
        fs = fopen(argv[1], "r");
        lpTex = texdata;
        if (fs) {
            fread(lpTex, 1, 1280 * 720 * 2, fs);
            fclose(fs);
        }
    }


    #ifdef UMP

    if (argv[1] && strstr(argv[1],"uyvy")) {

        pixmap = (EGLNativePixmapType)create_pixmap_ump(WIDTH, HEIGHT, 8, 8, 8, 8, 0, 1, 1);
        if (pixmap == NULL)
            printf("create ump pixmap failed\n");

        EGL_CHECK(egl_image = (EGLImageKHR)eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmap, NULL));
        if (egl_image == NULL)
            printf("create egl image failed\n");

        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex_id);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex_id);

        glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {

        glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, WIDTH, HEIGHT, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, texdata);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    #else

    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, WIDTH, HEIGHT, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, texdata);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    #endif

    if (argv[1] && strstr(argv[1],"uyvy")) {
        #ifdef UMP
        hProgramHandle = create_program(vertShader, fragShader_oes);
        #else
        hProgramHandle = create_program(vertShader, fragShader_uyvy);
        #endif
    } else {
        hProgramHandle = create_program(vertShader, fragShader);
    }

    mvp_pos = glGetUniformLocation(hProgramHandle, "mvp");

    glUseProgram(hProgramHandle);

    j = glGetError();
    if (j != GL_NO_ERROR) {
        printf("GL ERROR = %x", j);
        return GL_FALSE;
    }

    attriblocation = glGetAttribLocation(hProgramHandle, "position");
    glEnableVertexAttribArray(attriblocation);
    glVertexAttribPointer(attriblocation, 2, GL_FIXED, 0, 0, vertices);

    attriblocation = glGetAttribLocation(hProgramHandle, "inputtexcoord");
    glEnableVertexAttribArray(attriblocation);
    glVertexAttribPointer(attriblocation, 2, GL_FLOAT, 0, 0, texcoord);

    return GL_TRUE;
}

static void render(void)
{
    static int framecount = 0;

    glClear(GL_COLOR_BUFFER_BIT);

    Identity(projection);

    //Orthographic(projection, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f);

    Perspective(projection, 90, 1, 0, 1);

    glUseProgram(hProgramHandle);

    Identity(modelview);

    Translate(modelview, 0, 0, -0.5);

    Rotate(modelview, 1, 0, 0, framecount/1.0);
    Rotate(modelview, 0, 1, 0, framecount/1.0);
    Rotate(modelview, 0, 0, 1, framecount/1.0);

    MultiplyMatrix(mvp, modelview, projection);
    glUniformMatrix4fv(mvp_pos, 1, GL_FALSE, &mvp[0][0]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    framecount++;
}

static int handle_events(void)
{
    return 0;
}

int create_program(const char* v, const char* f)
{
    char pszInfoLog[1024];
    int  nShaderStatus, nInfoLogLength;

    //
    int vertshaderhandle = glCreateShader(GL_VERTEX_SHADER);
    int fragshaderhandle = glCreateShader(GL_FRAGMENT_SHADER);
    int programhandle = glCreateProgram();

    //
    char *pszProgramString0 = (char*)v;
    int  nProgramLength0 = strlen(v);
    glShaderSource(vertshaderhandle, 1, (const char **)&pszProgramString0, &nProgramLength0);
    glCompileShader(vertshaderhandle);
    glGetShaderiv(vertshaderhandle, GL_COMPILE_STATUS, &nShaderStatus);
    if (nShaderStatus != GL_TRUE)
    {
        printf("Error: Failed to compile GLSL shader\n");
        glGetShaderInfoLog(vertshaderhandle, 1024, &nInfoLogLength, pszInfoLog);
        printf("%s", pszInfoLog);
    }
    glAttachShader(programhandle, vertshaderhandle);

    //
    char *pszProgramString1 = (char*)f;
    int  nProgramLength1 = strlen(f);
    glShaderSource(fragshaderhandle, 1, (const char **)&pszProgramString1, &nProgramLength1);
    glCompileShader(fragshaderhandle);
    glGetShaderiv(fragshaderhandle, GL_COMPILE_STATUS, &nShaderStatus);
    if (nShaderStatus != GL_TRUE)
    {
        printf("Error: Failed to compile GLSL shader\n");
        glGetShaderInfoLog(fragshaderhandle, 1024, &nInfoLogLength, pszInfoLog);
        printf("%s", pszInfoLog);
    }
    glAttachShader(programhandle, fragshaderhandle);

    glLinkProgram(programhandle);
    glGetProgramiv(programhandle, GL_LINK_STATUS, &nShaderStatus);
    if (nShaderStatus != GL_TRUE)
    {
        printf("Error: Failed to link GLSL program\n");
        glGetProgramInfoLog(programhandle, 1024, &nInfoLogLength, pszInfoLog);
        printf("%s", pszInfoLog);
    }

    glValidateProgram(programhandle);
    glGetProgramiv(programhandle, GL_VALIDATE_STATUS, &nShaderStatus);
    if (nShaderStatus != GL_TRUE)
    {
        printf("Error: Failed to validate GLSL program\n");
        glGetProgramInfoLog(programhandle, 1024, &nInfoLogLength, pszInfoLog);
        printf("%s", pszInfoLog);
    }

    return programhandle;
}


int main(int argc, char **argv)
{
    int i;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    int   fbdev;

    struct fb_var_screeninfo varInfo;

    if ((fbdev = open("/dev/fb0", O_RDONLY)) == 0) {
        printf("Error opening %s\n", "/dev/fb0");
        return -1;
    }

    if (ioctl(fbdev, FBIOGET_VSCREENINFO, &varInfo) < 0) {
        printf("Error ioctl\n");
        return -1;
    }

    varInfo.xres;
    varInfo.yres;

    close(fbdev);

    egl_init(&display, &surface, &context, varInfo.xres, varInfo.yres);

    signal(SIGINT, &signal_handler);
    signal(SIGTERM, &signal_handler);
    signal(SIGHUP, &signal_handler);

    if (init(display, argc, argv) == GL_FALSE)
        goto term;

    glClearColor(0.5, 0.0, 0.0, 1.0);

    while (!frameStop)
    {
        render();

        egl_swap(display, surface);
    }

term:

    egl_exit(display, surface, context);

    return 0;
}
