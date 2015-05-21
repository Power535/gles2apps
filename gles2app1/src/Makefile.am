# Automake configuration file

bin_PROGRAMS = gles2app1

AM_CPPFLAGS = \
    $(egl_CFLAGS) \
    $(glesv2_CFLAGS)

gles2app1_LDADD = \
    $(egl_LIBS) \
    $(glesv2_LIBS)

gles2app1_SOURCES = \
    gles2app1.c

if IS586
AM_CPPFLAGS += $(gdl_CFLAGS)
gles2app1_LDADD += $(gdl_LIBS)
endif
