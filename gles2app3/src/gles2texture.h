#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define FLOAT_TO_FIXED(x) (long)((x)*65536.0f)

static const char *const defaultvertShader = "\n\
                                                      \n\
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
precision mediump float;                              \n\
                                                      \n\
varying vec2 texcoord;                                \n\
                                                      \n\
uniform sampler2D basetexture;                        \n\
                                                      \n\
void main(void)                                       \n\
{                                                     \n\
   gl_FragColor = texture2D(basetexture, texcoord);   \n\
}                                                     \n";

class Texture {

  public:
    void Create(float x = 0, float y = 0, float width = 1, float height = 1,
                int twidth = 320, int theight = 180, char *filename = 0,
                bool usebuffers = false);

    void Draw();

  private:
    int id;

    int m_drawcount;

    float m_x;
    float m_y;

    float m_width;
    float m_height;

    const char *m_fragshader;
    int m_fragshaderhandle;

    const char *m_vertshader;
    int m_vertshaderhandle;

    int m_programhandle;

    GLuint m_textureid;

    int m_mvp_pos;
    int m_attriblocation_position;
    int m_attriblocation_inputtexcoord;

    GLubyte *m_texdata;
    GLfloat *m_a_verts;
    GLfloat *m_e_verts;
    GLushort *m_inds;
    GLfloat *m_texcoords;
    bool m_usebuffers;
    GLuint m_e_vertsbuffer;
    GLuint m_indsbuffer;
    GLuint m_texcoordsbuffer;
};
