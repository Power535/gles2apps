# Automake configuration file

bin_PROGRAMS = gles2app5

AM_CPPFLAGS = \
    $(egl_CFLAGS) \
    $(glesv2_CFLAGS)

gles2app5_LDADD = \
    $(egl_LIBS) \
    $(glesv2_LIBS) -lpthread

gles2app5_SOURCES = \
    gles2app5.c

if IS686
AM_CPPFLAGS += $(gdl_CFLAGS)
gles2app5_LDADD += $(gdl_LIBS)
endif
