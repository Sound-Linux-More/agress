bin_PROGRAMS = agcodec agplay

# set the include path found by configure
INCLUDES = `pkg-config glib-2.0 --cflags` $(all_includes)

# the library search path.
agcodec_SOURCES =  agcodec.c agress.h
agcodec_LDADD = -lglib-2.0 libagress.la -lpopt
agplay_SOURCES =  agplay.c agress.h
agplay_LDADD = -lglib-2.0 libagress.la

pkglib_LTLIBRARIES = libagress.la
libagress_la_SOURCES = agress.c agress.h
libagress_la_LDFLAGS = -version-info 1:2:1 -no-undefined
include_HEADERS = agress.h
