#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <gles2texture.h>
#include <gles2math.h>
#include <gles2util.h>

#include <png.h>


static int global_id = 0;

int read_png_file(char* file_name, char** ppbytes, int* width, int* height)
{
    int             x, y;

    png_byte        color_type;
    png_byte        bit_depth;

    png_structp     png_ptr;
    png_infop       info_ptr;
    int             number_of_passes;
    png_bytep       *row_pointers;

    char            *map_base;
    char            header[8];


    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        printf("[read_png_file] File %s could not be opened for reading\n", file_name);
        return -1;
    }

    fread(header, 1, 8, fp);
    if (png_sig_cmp((unsigned char*)header, 0, 8)) {
        printf("[read_png_file] File %s is not recognized as a PNG file\n", file_name);
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

    row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * *height);
    for (y = 0; y < *height; y++)
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr,info_ptr));

    png_read_image(png_ptr, row_pointers);

    fclose(fp);

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB) {
        printf("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA "
                        "(lacks the alpha channel)");
        return -1;
    }

    if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGBA) {
        printf("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
                        PNG_COLOR_TYPE_RGBA, png_get_color_type(png_ptr, info_ptr));
        return -1;
    }

    char* bytes = (char*)malloc(*width * *height * 4);

    for (y = 0 ; y < *height ; y++)
    {
        png_byte* row = row_pointers[y];
        for (x = 0; x < *width; x++)
        {
            png_byte* ptr = &(row[x * 4]);

            bytes[( (*height - 1 - y) * *width + x) * 4 + 0] /* B */ = ptr[2];
            bytes[( (*height - 1 - y) * *width + x) * 4 + 1] /* G */ = ptr[1];
            bytes[( (*height - 1 - y) * *width + x) * 4 + 2] /* R */ = ptr[0];
            bytes[( (*height - 1 - y) * *width + x) * 4 + 3] /* A */ = ptr[3];
        }
    }

    *ppbytes = bytes;
    return 0;
}


void Texture::Create(float x, float y, float width, float height, int twidth, int theight, char* filename)
{
    id = global_id++;

    m_drawcount = 0;

    m_x = x;
    m_y = y;

    m_width = width;
    m_height = height;

    m_vertshader = defaultvertShader;
    m_fragshader = defaultfragShader;

    int i,j;
    char pszInfoLog[1024];
    int  nShaderStatus, nInfoLogLength;


    int pngwidth;
    int pngheight;

    if (read_png_file(filename, (char**)&m_texdata, &pngwidth, &pngheight) != -1) {

        if (m_width == 0) {
            m_width = pngwidth / 1280.0;
        }

        if (m_height == 0) {
            m_height = pngheight / 720.0;
        }

        printf("texture = [%s], %dx%d\n", filename, twidth, theight);
    }
    else {

        m_texdata = (GLubyte*) malloc (twidth * theight * 4);

        printf("texture = checkerboard pattern, %dx%d\n", twidth, theight);

        GLubyte* lpTex = m_texdata;
        for (j = 0; j < theight; j++) {
            for (i = 0; i < twidth; i++) {
                if ((i ^ j) & 0x8) {
                    lpTex[0] = 0x00;
                    lpTex[1] = 0x80;
                    lpTex[2] = 0x00;
                } else {
                    lpTex[0] = 0x80;
                    lpTex[1] = 0x00;
                    lpTex[2] = 0x00;
                }
                lpTex[3] = 0xff;
                lpTex += 4;
            }
        }

    }


    // TRIANGLE FAN
    m_vertices = new GLfixed [8];

    m_vertices[0] = FLOAT_TO_FIXED(m_width); m_vertices[1] =  FLOAT_TO_FIXED(0.0),
    m_vertices[2] = FLOAT_TO_FIXED(0.0);     m_vertices[3] =  FLOAT_TO_FIXED(0.0),
    m_vertices[4] = FLOAT_TO_FIXED(0.0);     m_vertices[5] =  FLOAT_TO_FIXED(m_height),
    m_vertices[6] = FLOAT_TO_FIXED(m_width); m_vertices[7] =  FLOAT_TO_FIXED(m_height),

    m_texcoords = new GLfloat [8];

    m_texcoords[0] = 1.0; m_texcoords[1] = 0.0;
    m_texcoords[2] = 0.0; m_texcoords[3] = 0.0;
    m_texcoords[4] = 0.0; m_texcoords[5] = 1.0;
    m_texcoords[6] = 1.0; m_texcoords[7] = 1.0;

    glGenTextures(1, &m_textureid);

    glBindTexture(GL_TEXTURE_2D, m_textureid);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, twidth, theight, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, m_texdata);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    m_programhandle = create_program(m_vertshader, m_fragshader);

    glBindAttribLocation(m_programhandle, 0, "position");

    glUseProgram(m_programhandle);

    j = glGetError();
    if (j != GL_NO_ERROR) {
        printf("GL ERROR = %x", j);
    }
}


void Texture::Draw()
{
    GLfloat modelview[4][4]  = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    GLfloat projection[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    GLfloat mvp[4][4]        = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};

    Identity(modelview);

    //Translate(modelview, -0.5 + fabs(sin(m_drawcount * DegreesToRadians)), 0, 0);
    Translate(modelview, m_x  + 0.01 * sin((m_drawcount * 1 + id * 360 / (global_id + 1)) * DegreesToRadians) ,
                         m_y  + 0.01 * cos((m_drawcount * 1 + id * 360 / (global_id + 1)) * DegreesToRadians) , 0);


    /*
     * Rotate(modelview, 0, 0, 1, m_drawcount);
     */

    Identity(projection);
    Orthographic(projection, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    MultiplyMatrix(mvp, modelview, projection);

    m_mvp_pos = glGetUniformLocation(m_programhandle, "mvp");
    glUniformMatrix4fv(m_mvp_pos, 1, GL_FALSE, &mvp[0][0]);


    glBindTexture(GL_TEXTURE_2D, m_textureid);

    glEnableVertexAttribArray(m_attriblocation);

    glUseProgram(m_programhandle);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);


    glEnableVertexAttribArray(0);
    m_attriblocation = glGetAttribLocation(m_programhandle, "position");
    glVertexAttribPointer(m_attriblocation, 2, GL_FIXED, 0, 0, m_vertices);
    //glVertexAttribPointer(0, 2, GL_FIXED, 0, 0, m_vertices);

    glEnableVertexAttribArray(1);
    m_attriblocation = glGetAttribLocation(m_programhandle, "inputtexcoord");
    glVertexAttribPointer(m_attriblocation, 2, GL_FLOAT, 0, 0, m_texcoords);
    //glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, m_texcoords);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    m_drawcount++;
}
