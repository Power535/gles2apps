# Automake configuration file

bin_PROGRAMS = gles2app3

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

if IS586
AM_CPPFLAGS += $(gdl_CFLAGS)
gles2app3_LDADD += $(gdl_LIBS)
endif
