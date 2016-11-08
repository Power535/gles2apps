#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "gles2texture.h"
#include "gles2math.h"
#include "gles2util.h"

#include <png.h>

static const char *const defaultvertShader = "\n\
varying vec2 texcoord;                                \n\
                                                      \n\
attribute vec3 position;                              \n\
attribute vec2 inputtexcoord;                         \n\
                                                      \n\
uniform mat4 mvp;                                     \n\
                                                      \n\
void main(void)                                       \n\
{                                                     \n\
   texcoord = inputtexcoord;                          \n\
                                                      \n\
   gl_Position = mvp * vec4(position, 1.0);           \n\
}                                                     \n";

static const char *const defaultfragShader = "\n\
varying vec2 texcoord;                                \n\
                                                      \n\
uniform sampler2D basetexture;                        \n\
                                                      \n\
void main(void)                                       \n\
{                                                     \n\
   gl_FragColor = texture2D(basetexture, texcoord);   \n\
}                                                     \n";

static int global_id = 0;

int read_png_file(char *file_name, char **ppbytes, int *width, int *height) {
    int x, y;

    png_byte color_type;
    png_byte bit_depth;

    png_structp png_ptr;
    png_infop info_ptr;
    int number_of_passes;
    png_bytep *row_pointers;

    char *map_base;
    char header[8];

    if (!file_name)
        return -1;

    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        printf("[read_png_file] File %s could not be opened for reading\n",
               file_name);
        return -1;
    }

    fread(header, 1, 8, fp);
    if (png_sig_cmp((unsigned char *)header, 0, 8)) {
        printf("[read_png_file] File %s is not recognized as a PNG file\n",
               file_name);
        return -1;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr) {
        printf("[read_png_file] png_create_read_struct failed\n");
        return -1;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        printf("[read_png_file] png_create_info_struct failed");
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        printf("[read_png_file] Error during init_io");
        return -1;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);

    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    number_of_passes = png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    /* read file */
    if (setjmp(png_jmpbuf(png_ptr))) {
        printf("[read_png_file] Error during read_image\n");
        return -1;
    }

    row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * *height);
    for (y = 0; y < *height; y++)
        row_pointers[y] =
            (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));

    png_read_image(png_ptr, row_pointers);

    fclose(fp);

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB) {
        printf("[process_file] input file is PNG_COLOR_TYPE_RGB but must be "
               "PNG_COLOR_TYPE_RGBA "
               "(lacks the alpha channel)");
        return -1;
    }

    if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGBA) {
        printf("[process_file] color_type of input file must be "
               "PNG_COLOR_TYPE_RGBA (%d) (is %d)",
               PNG_COLOR_TYPE_RGBA, png_get_color_type(png_ptr, info_ptr));
        return -1;
    }

    char *bytes = (char *)malloc(*width * *height * 4);

    for (y = 0; y < *height; y++) {
        png_byte *row = row_pointers[y];
        for (x = 0; x < *width; x++) {
            png_byte *ptr = &(row[x * 4]);

            bytes[((*height - 1 - y) * *width + x) * 4 + 0] /* B */ = ptr[2];
            bytes[((*height - 1 - y) * *width + x) * 4 + 1] /* G */ = ptr[1];
            bytes[((*height - 1 - y) * *width + x) * 4 + 2] /* R */ = ptr[0];
            bytes[((*height - 1 - y) * *width + x) * 4 + 3] /* A */ = ptr[3];
        }
    }

    *ppbytes = bytes;
    return 0;
}

void Texture::Create(float x, float y, float width, float height, int twidth,
                     int theight, char *filename, bool usebuffers) {

    m_usebuffers = usebuffers;

    id = global_id++;

    m_drawcount = 0;

    m_x = x;
    m_y = y;

    m_width = width;
    m_height = height;

    m_vertshader = defaultvertShader;
    m_fragshader = defaultfragShader;

    int i, j;
    char pszInfoLog[1024];
    int nShaderStatus, nInfoLogLength;

    int pngwidth;
    int pngheight;

    if (read_png_file(filename, (char **)&m_texdata_rgba, &pngwidth, &pngheight) !=
        -1) {

        if (m_width == 0) {
            m_width = pngwidth / 1280.0;
        }

        if (m_height == 0) {
            m_height = pngheight / 720.0;
        }

    } else {

        m_texdata_rgba = (GLubyte *)malloc(twidth * theight * 4);
        m_texdata_bgra = (GLubyte *)malloc(twidth * theight * 4);

        GLubyte *lpTex_rgba = m_texdata_rgba;
        GLubyte *lpTex_bgra = m_texdata_bgra;
        for (j = 0; j < theight; j++) {
            for (i = 0; i < twidth; i++) {
                if ((i ^ j) & 0x40) {
                    lpTex_rgba[0] = lpTex_rgba[1] = lpTex_rgba[2] = 0x00;
                    lpTex_rgba[3] = 0x00;
                } else if ((i ^ j) & 0x20) {
                    lpTex_rgba[0] = lpTex_rgba[1] = lpTex_rgba[2] = 0xff;
                    lpTex_rgba[3] = 0xdf;
                } else {
                    lpTex_rgba[0] = lpTex_rgba[1] = 0x00;
                    lpTex_rgba[2] = 0xff;
                    lpTex_rgba[3] = 0xdf;
                }

                lpTex_bgra[0] = lpTex_rgba[2];
                lpTex_bgra[1] = lpTex_rgba[1];
                lpTex_bgra[2] = lpTex_rgba[0];
                lpTex_bgra[3] = lpTex_rgba[3];

                lpTex_rgba += 4;
                lpTex_bgra += 4;
            }
        }

    }

    /*
     * glDrawArrays TRIANGLE FAN
     *  x  y  z
     *  1, 0, 0, bottom right corner
     *  0, 0, 0, bottom left corner
     *  0, 1, 0, top left corner
     *  1, 1, 0  top right corner
     */
    m_a_verts = new GLfloat[12];
    m_a_verts[0] = m_width;
    m_a_verts[1] = 0.0;
    m_a_verts[2] = 0.0;
    m_a_verts[3] = 0.0;
    m_a_verts[4] = 0.0;
    m_a_verts[5] = 0.0;
    m_a_verts[6] = 0.0;
    m_a_verts[7] = m_height;
    m_a_verts[8] = 0.0;
    m_a_verts[9] = m_width;
    m_a_verts[10] = m_height;
    m_a_verts[11] = 0.0;

    /*
     * glDrawElements TRIANGLES
     *
     * 0, 0, 0,   bottom left corner
     * 0, 1, 0,   top left corner
     * 1, 1, 0,   top right corner
     * 1, 0, 0   bottom right corner
     *
     */
    m_e_verts = new GLfloat[12];
    m_e_verts[0] = 0.0;
    m_e_verts[1] = 0.0;
    m_e_verts[2] = 0.0;
    m_e_verts[3] = 0.0;
    m_e_verts[4] = m_height;
    m_e_verts[5] = 0.0;
    m_e_verts[6] = m_width;
    m_e_verts[7] = m_height;
    m_e_verts[8] = 0.0;
    m_e_verts[9] = m_width;
    m_e_verts[10] = 0.0;
    m_e_verts[11] = 0.0;

    if (m_usebuffers) {
        glGenBuffers(1, &m_e_vertsbuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_e_vertsbuffer);
        glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), m_e_verts,
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    m_inds = new GLushort[6];
    // first triangle (bottom left - top left - top right)
    // second triangle (bottom left - top right - bottom right)
    m_inds[0] = 0;
    m_inds[1] = 1;
    m_inds[2] = 2;
    m_inds[3] = 0;
    m_inds[4] = 2;
    m_inds[5] = 3;

    if (m_usebuffers) {
        glGenBuffers(1, &m_indsbuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indsbuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLushort), m_inds,
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    m_texcoords = new GLfloat[8];
    m_texcoords[0] = 1.0;
    m_texcoords[1] = 0.0;
    m_texcoords[2] = 0.0;
    m_texcoords[3] = 0.0;
    m_texcoords[4] = 0.0;
    m_texcoords[5] = 1.0;
    m_texcoords[6] = 1.0;
    m_texcoords[7] = 1.0;

    if (m_usebuffers) {
        glGenBuffers(1, &m_texcoordsbuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_texcoordsbuffer);
        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), m_texcoords,
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    glGenTextures(1, &m_textureid);

    glBindTexture(GL_TEXTURE_2D, m_textureid);

    #if 0
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, twidth, theight, 0, GL_BGRA_EXT,
                 GL_UNSIGNED_BYTE, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, twidth, theight, GL_BGRA_EXT,
                    GL_UNSIGNED_BYTE, m_texdata_bgra);
    #else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, twidth, theight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, twidth, theight, GL_RGBA,
                    GL_UNSIGNED_BYTE, m_texdata_rgba);
    #endif


    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_programhandle = create_program(m_vertshader, m_fragshader);

    glBindAttribLocation(m_programhandle, 0, "position");
    glBindAttribLocation(m_programhandle, 1, "inputtexcoord");
}

void Texture::Draw() {
    GLenum err;
    GLfloat modelview[4][4] = {
        {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    GLfloat projection[4][4] = {
        {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    GLfloat mvp[4][4] = {
        {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

    Identity(modelview);
    Translate(modelview,
              m_x +
                  0.01 * sin((m_drawcount * 1 + id * 360 / (global_id + 1)) *
                             DegreesToRadians),
              m_y +
                  0.01 * cos((m_drawcount * 1 + id * 360 / (global_id + 1)) *
                             DegreesToRadians),
              0);

    Identity(projection);
    Orthographic(projection, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    MultiplyMatrix(mvp, modelview, projection);

    glUseProgram(m_programhandle);
    m_mvp_pos = glGetUniformLocation(m_programhandle, "mvp");
    glUniformMatrix4fv(m_mvp_pos, 1, GL_FALSE, &mvp[0][0]);

    m_attriblocation_position =
        glGetAttribLocation(m_programhandle, "position");
    glEnableVertexAttribArray(m_attriblocation_position);

    if (m_usebuffers) {
        glBindBuffer(GL_ARRAY_BUFFER, m_e_vertsbuffer);
        glVertexAttribPointer(m_attriblocation_position, 3, GL_FLOAT, 0, 0, 0);

    } else {
        // glVertexAttribPointer(m_attriblocation_position, 3, GL_FLOAT, 0, 0,
        // m_a_verts);
        glVertexAttribPointer(m_attriblocation_position, 3, GL_FLOAT, 0, 0,
                              m_e_verts);
    }

    m_attriblocation_inputtexcoord =
        glGetAttribLocation(m_programhandle, "inputtexcoord");
    glEnableVertexAttribArray(m_attriblocation_inputtexcoord);
    if (m_usebuffers) {
        glBindBuffer(GL_ARRAY_BUFFER, m_texcoordsbuffer);
        glVertexAttribPointer(m_attriblocation_inputtexcoord, 2, GL_FLOAT, 0, 0,
                              0);
    } else {
        glVertexAttribPointer(m_attriblocation_inputtexcoord, 2, GL_FLOAT, 0, 0,
                              m_texcoords);
    }

    glBindTexture(GL_TEXTURE_2D, m_textureid);

    glUseProgram(m_programhandle);

    if (m_usebuffers) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indsbuffer);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    } else {
        // glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, m_inds);
    }

    m_drawcount++;

    glDisableVertexAttribArray(m_attriblocation_position);
    glDisableVertexAttribArray(m_attriblocation_inputtexcoord);
}
