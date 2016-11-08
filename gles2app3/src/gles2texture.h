#define FLOAT_TO_FIXED(x) (long)((x)*65536.0f)

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

    GLubyte *m_texdata_rgba;
    GLubyte *m_texdata_bgra;
    GLfloat *m_a_verts;
    GLfloat *m_e_verts;
    GLushort *m_inds;
    GLfloat *m_texcoords;
    bool m_usebuffers;
    GLuint m_e_vertsbuffer;
    GLuint m_indsbuffer;
    GLuint m_texcoordsbuffer;
};
