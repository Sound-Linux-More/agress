bin_PROGRAMS = agcodec agplay

# set the include path found by configure
INCLUDES = `pkg-config glib-2.0 --cflags` $(all_includes)

# the library search path.
agcodec_LDADD = -lglib-2.0 -lpopt
agcodec_SOURCES =  agcodec.c agress.c agress.h
agplay_SOURCES =  agplay.c agress.c agress.h
agplay_LDADD = -lglib-2.0
