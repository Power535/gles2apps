# Automake configuration file

bin_PROGRAMS = gles2app2

AM_CPPFLAGS = \
    $(egl_CFLAGS) \
    $(glesv2_CFLAGS)

gles2app2_LDADD = \
    $(egl_LIBS) \
    $(glesv2_LIBS)

gles2app2_SOURCES = \
    gles2app2.c

if IS586
AM_CPPFLAGS += $(gdl_CFLAGS)
gles2app2_LDADD += $(gdl_LIBS)
endif
