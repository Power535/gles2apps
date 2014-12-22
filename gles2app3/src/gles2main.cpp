#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#include <sys/time.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#include <gles2texture.h>
#include <gles2math.h>
#include <gles2egl.h>
#include <gles2text.h>


static int Stop = 0;

static void signal_handler(int sig_num)
{
    if (sig_num == SIGINT || sig_num == SIGTERM || sig_num == SIGHUP) {
        Stop = 1;
    }
}


typedef unsigned long long  _u64;

static _u64 timestamp_usec(void)
{
    struct timeval  mytime;
    gettimeofday(&mytime, 0);
    return ((_u64)((_u64)mytime.tv_usec + (_u64)mytime.tv_sec * (_u64)1000000));
}


int main(int argc, char *argv[])
{
    int i, x, y;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    egl_init(&display, &surface, &context);

    signal(SIGINT, &signal_handler);
    signal(SIGTERM, &signal_handler);
    signal(SIGHUP, &signal_handler);

    #define W 8
    #define H 8

    Texture textures[ W * H ];

    for (x = 0; x < W; x++) {

        for (y = 0; y < H; y++) {

            textures[y * W + x].Create((1.0 * x) / W, (1.0 * y) / H, 1.0 / W, 1.0 / H, 640, 480, argv[1]);
        }
    }


    init_resources();


    _u64 first_timestamp = timestamp_usec();

    _u64 prev_timestamp = 0;

    _u64 current;

    _u64 longstanding_longest = 0;
    _u64 longstanding_shortest = 0;

    _u64 longest = 0;
    _u64 shortest = 0;


    int count = 0;

    glClearColor(0.5, 0.0, 0.0, 1.0);

    while (!Stop)
    {

        _u64 timestamp = timestamp_usec();

        glClear(GL_COLOR_BUFFER_BIT);

        for (i = 0; i < (W * H) ; i++) {

            textures[i].Draw();
        }

        if (prev_timestamp) {

            current = timestamp - prev_timestamp;

            if ((longstanding_shortest == 0) || (current < longstanding_shortest)) longstanding_shortest = current;
            if ((longstanding_longest == 0) || (current > longstanding_longest)) longstanding_longest = current;

            if ((shortest == 0) || (current < shortest)) shortest = current;
            if ((longest == 0) || (current > longest)) longest = current;

            render_text(0, 0, "%2.1f-%2.1f,%2.1f-%2.1f,%2.1f[%2.1f]",
                    1000000.0f / longstanding_longest,
                    1000000.0f / longstanding_shortest,
                    1000000.0f / longest,
                    1000000.0f / shortest,
                    1000000.0f / current,

                    count * 1000000.0f / (timestamp - first_timestamp)

                    );
        }
        prev_timestamp = timestamp;

        egl_swap(display, surface);

        count++;
        if ((count % 60) == 0) {

            longest = 0;
            shortest = 0;

        }

    }


    egl_exit(display, surface, context);

    return 0;
}
