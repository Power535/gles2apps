# Automake configuration file

bin_PROGRAMS = gles2app6

AM_CPPFLAGS = \
    $(egl_CFLAGS) \
    $(glesv2_CFLAGS)

gles2app6_LDADD = \
    $(egl_LIBS) \
    $(glesv2_LIBS) -lpthread

gles2app6_SOURCES = \
    gles2app6.c

if IS686
AM_CPPFLAGS += $(gdl_CFLAGS)
gles2app6_LDADD += $(gdl_LIBS)
endif
