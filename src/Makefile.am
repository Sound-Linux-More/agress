bin_PROGRAMS = agcodec agplay

# set the include path found by configure
AM_CPPFLAGS = -DGLIB_COMPILATION `pkg-config glib-2.0 --cflags` $(all_includes)

# the library search path.
lib_LTLIBRARIES = libagress.la
libagress_la_SOURCES = agress.c agress.h
libagress_la_LDFLAGS = -version-info 1:3:1 -no-undefined
include_HEADERS = agress.h

agcodec_SOURCES =  agcodec.c agress.h
agcodec_LDADD = libagress.la -lglib-2.0 -lpopt
agplay_SOURCES =  agplay.c agress.h
agplay_LDADD = libagress.la -lglib-2.0

