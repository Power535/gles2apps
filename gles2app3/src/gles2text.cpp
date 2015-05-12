#include <cmath>
#include <algorithm>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdint.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "gles2util.h"

#include "Vera.ttf.c"

static const char* const TextvertShader =                               "\n\
                                                                        \n\
attribute vec4 coord;                                                   \n\
varying vec2 texpos;                                                    \n\
                                                                        \n\
void main(void)                                                         \n\
{                                                                       \n\
    gl_Position = vec4(coord.xy, 0, 1);                                 \n\
    texpos = coord.zw;                                                  \n\
}                                                                       \n";


static const char* const TextfragShader =                               "\n\
                                                                        \n\
varying highp vec2 texpos;                                              \n\
uniform sampler2D tex;                                                  \n\
uniform highp vec4 color;                                               \n\
                                                                        \n\
void main(void)                                                         \n\
{                                                                       \n\
    gl_FragColor = vec4(1, 1, 1, texture2D(tex, texpos).a) * color;     \n\
}                                                                       \n";



GLuint program;
GLint attribute_coord;
GLint uniform_tex;
GLint uniform_color;

struct point
{
    GLfloat x;
    GLfloat y;
    GLfloat s;
    GLfloat t;
};

GLuint vbo;

FT_Library ft;
FT_Face face;

// Maximum texture width
#define MAXWIDTH 1024

/**
 * The atlas struct holds a texture that contains the visible US-ASCII characters
 * of a certain font rendered with a certain character height.
 * It also contains an array that contains all the information necessary to
 * generate the appropriate vertex and texture coordinates for each character.
 *
 * After the constructor is run, you don't need to use any FreeType functions anymore.
 */
struct atlas
{
    GLuint tex;        // texture object

    int w;            // width of texture in pixels
    int h;            // height of texture in pixels

    struct
    {
        float ax;    // advance.x
        float ay;    // advance.y

        float bw;    // bitmap.width;
        float bh;    // bitmap.height;

        float bl;    // bitmap_left;
        float bt;    // bitmap_top;

        float tx;    // x offset of glyph in texture coordinates
        float ty;    // y offset of glyph in texture coordinates
    } c[128];        // character information

    atlas(FT_Face face, int height)
    {
        FT_Set_Pixel_Sizes(face, 0, height);
        FT_GlyphSlot g = face->glyph;

        int roww = 0;
        int rowh = 0;
        w = 0;
        h = 0;

        memset(c, 0, sizeof c);

        /* Find minimum size for a texture holding all visible ASCII characters */
        for (int i = 32; i < 128; i++) {
            if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
                fprintf(stderr, "Loading character %c failed!\n", i);
                continue;
            }
            if (roww + g->bitmap.width + 1 >= MAXWIDTH) {
                w = std::max(w, roww);
                h += rowh;
                roww = 0;
                rowh = 0;
            }
            roww += g->bitmap.width + 1;
            rowh = std::max(rowh, static_cast<int>(g->bitmap.rows));
        }

        w = std::max(w, roww);
        h += rowh;

        /* Create a texture that will be used to hold all ASCII glyphs */
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(uniform_tex, 0);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);

        /* We require 1 byte alignment when uploading texture data */
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        /* Clamping to edges is important to prevent artifacts when scaling */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        /* Linear filtering usually looks best for text */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        /* Paste all glyph bitmaps into the texture, remembering the offset */
        int ox = 0;
        int oy = 0;

        rowh = 0;

        for (int i = 32; i < 128; i++) {
            if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
                fprintf(stderr, "Loading character %c failed!\n", i);
                continue;
            }

            if (ox + g->bitmap.width + 1 >= MAXWIDTH) {
                oy += rowh;
                rowh = 0;
                ox = 0;
            }

            glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy, g->bitmap.width, g->bitmap.rows, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);
            c[i].ax = g->advance.x >> 6;
            c[i].ay = g->advance.y >> 6;

            c[i].bw = g->bitmap.width;
            c[i].bh = g->bitmap.rows;

            c[i].bl = g->bitmap_left;
            c[i].bt = g->bitmap_top;

            c[i].tx = ox / (float)w;
            c[i].ty = oy / (float)h;

            rowh = std::max(rowh, static_cast<int>(g->bitmap.rows));
            ox += g->bitmap.width + 1;
        }

        fprintf(stderr, "Generated a %d x %d (%d kb) texture atlas\n", w, h, w * h / 1024);
    }

    ~atlas() {
        glDeleteTextures(1, &tex);
    }
};

atlas* a48;

#if 0
atlas* a24;
atlas* a12;
#endif

int init_resources()
{
    /* Initialize the FreeType2 library */
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init freetype library\n");
        return 0;
    }

    if (FT_New_Memory_Face(ft, (FT_Byte*)Vera_ttf, Vera_ttf_len, 0, &face)) {
        fprintf(stderr, "Could not load font\n");
        return 0;
    }

    program = create_program(TextvertShader, TextfragShader);
    if (program == 0)
        return 0;

    glBindAttribLocation(program, 0, "coord");

    attribute_coord = glGetAttribLocation(program, "coord");
    uniform_tex = glGetUniformLocation(program, "tex");
    uniform_color = glGetUniformLocation(program, "color");

    if (attribute_coord == -1 || uniform_tex == -1 || uniform_color == -1)
        return 0;

    // Create the vertex buffer object
    glGenBuffers(1, &vbo);

    /* Create texture atlasses for several font sizes */
    a48 = new atlas(face, 48);

    #if 0
    a24 = new atlas(face, 24);
    a12 = new atlas(face, 12);
    #endif

    return 1;
}

/**
 * Render text using the currently loaded font and currently set font size.
 * Rendering starts at coordinates (x, y), z is always 0.
 * The pixel coordinates that the FreeType2 library uses are scaled by (sx, sy).
 */
void render_text(float x, float y, const char* s, ...)
{
    char text[256];
    const uint8_t* p;

    va_list args;
    va_start(args, s);
    vsprintf(text, s, args);
    va_end(args);


    float sx = 2.0 / 1280;
    float sy = 2.0 / 720;

    glUseProgram(program);

    /* Enable blending, necessary for our alpha texture */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLfloat white[4] = { 1, 1, 1, 1 };

    glUniform4fv(uniform_color, 1, white);


    //render_text(a48, -1 + 1000 * sx, 1 - 680 * sy, sx, sy);

    atlas* a = a48;
    x = -1 + 100 * sx;
    y =  1 - 680 * sy;


    /* Use the texture containing the atlas */
    glBindTexture(GL_TEXTURE_2D, a->tex);
    glUniform1i(uniform_tex, 0);

    /* Set up the VBO for our vertex data */
    glEnableVertexAttribArray(attribute_coord);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);

    point coords[6 * strlen(text)];
    int c = 0;

    /* Loop through all characters */
    for (p = (const uint8_t *)text; *p; p++) {

        /* Calculate the vertex and texture coordinates */
        float x2 = x + a->c[*p].bl * sx;
        float y2 = -y - a->c[*p].bt * sy;
        float w = a->c[*p].bw * sx;
        float h = a->c[*p].bh * sy;

        /* Advance the cursor to the start of the next character */
        x += a->c[*p].ax * sx;
        y += a->c[*p].ay * sy;

        /* Skip glyphs that have no pixels */
        if (!w || !h)
            continue;

        coords[c++] = (point) {x2, -y2, a->c[*p].tx, a->c[*p].ty};
        coords[c++] = (point) {x2 + w, -y2, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty};
        coords[c++] = (point) {x2, -y2 - h, a->c[*p].tx, a->c[*p].ty + a->c[*p].bh / a->h};
        coords[c++] = (point) {x2 + w, -y2, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty};
        coords[c++] = (point) {x2, -y2 - h, a->c[*p].tx, a->c[*p].ty + a->c[*p].bh / a->h};
        coords[c++] = (point) {x2 + w, -y2 - h, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty + a->c[*p].bh / a->h};
    }

    /* Draw all the character on the screen in one go */
    glBufferData(GL_ARRAY_BUFFER, sizeof coords, coords, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, c);

    glDisableVertexAttribArray(attribute_coord);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void free_resources()
{
    glDeleteProgram(program);
}
