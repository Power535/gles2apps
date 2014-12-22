# Automake configuration file

bin_PROGRAMS = gles2app3

## gles2app3_texturedir = /usr/share/gles2apps/gles2app3/textures
## gles2app3_texture_DATA = \
##     ${srcdir}/../textures/bg-1280x720.png

AM_CPPFLAGS = \
    $(egl_CFLAGS) \
    $(glesv2_CFLAGS) \
    $(libpng_CFLAGS) \
    $(freetype2_CFLAGS)

gles2app3_LDADD = \
    $(egl_LIBS) \
    $(glesv2_LIBS) \
    $(libpng_LIBS) \
    $(freetype2_LIBS)

gles2app3_SOURCES = \
    gles2main.cpp \
    gles2texture.cpp \
    gles2texture.h \
    gles2math.cpp \
    gles2math.h \
    gles2egl.cpp \
    gles2egl.h \
    gles2util.cpp \
    gles2util.h \
    gles2text.cpp \
    gles2text.h

