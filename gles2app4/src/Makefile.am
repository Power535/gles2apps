# Automake configuration file

bin_PROGRAMS = gles2app4

AM_CPPFLAGS = \
    $(egl_CFLAGS) \
    $(glesv2_CFLAGS)

gles2app4_LDADD = \
    $(egl_LIBS) \
    $(glesv2_LIBS)

gles2app4_SOURCES = \
    gles2app4.c

if IS686
AM_CPPFLAGS += $(gdl_CFLAGS)
gles2app4_LDADD += $(gdl_LIBS)
endif
