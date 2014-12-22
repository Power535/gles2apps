#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define FLOAT_TO_FIXED(x) (long)((x) * 65536.0f)

static const char* const defaultvertShader =         "\n\
                                                      \n\
varying vec2 texcoord;                                \n\
                                                      \n\
attribute vec4 position;                              \n\
attribute vec2 inputtexcoord;                         \n\
                                                      \n\
uniform mat4 mvp;                                     \n\
                                                      \n\
void main(void)                                       \n\
{                                                     \n\
   texcoord = inputtexcoord;                          \n\
                                                      \n\
   gl_Position = mvp * position;                      \n\
}                                                     \n";


static const char* const defaultfragShader =         "\n\
                                                      \n\
varying highp vec2 texcoord;                          \n\
                                                      \n\
uniform sampler2D basetexture;                        \n\
                                                      \n\
void main(void)                                       \n\
{                                                     \n\
   gl_FragColor = texture2D(basetexture, texcoord);   \n\
}                                                     \n";


class Texture
{

   public:

      void Create(float x=0, float y=0, float width=1, float height=1, int twidth=320, int theight=180, char* filename=0);

      void Draw();

   private:

      int      id;

      int      m_drawcount;

      float    m_x;
      float    m_y;

      float    m_width;
      float    m_height;

      const char* m_fragshader;
      int      m_fragshaderhandle;

      const char* m_vertshader;
      int      m_vertshaderhandle;

      int      m_programhandle;

      GLuint   m_textureid;

      int      m_mvp_pos;
      int      m_attriblocation;


      GLubyte*  m_texdata;
      GLfixed*  m_vertices;
      GLfloat*  m_texcoords;

} ;


