noinst_LTLIBRARIES += libeditorconfig.la

libeditorconfig_la_SOURCES = \
	libide/editorconfig/libeditorconfig/editorconfig.c \
	libide/editorconfig/libeditorconfig/misc.c \
	libide/editorconfig/libeditorconfig/ini.c \
	libide/editorconfig/libeditorconfig/ec_glob.c \
	libide/editorconfig/libeditorconfig/editorconfig_handle.c \
	libide/editorconfig/libeditorconfig/editorconfig_handle.h \
	libide/editorconfig/libeditorconfig/editorconfig/editorconfig_handle.h \
	libide/editorconfig/libeditorconfig/editorconfig/editorconfig.h \
	libide/editorconfig/libeditorconfig/global.h \
	libide/editorconfig/libeditorconfig/ec_glob.h \
	libide/editorconfig/libeditorconfig/utarray.h \
	libide/editorconfig/libeditorconfig/editorconfig.h \
	libide/editorconfig/libeditorconfig/misc.h \
	libide/editorconfig/libeditorconfig/ini.h \
	$(NULL)

libeditorconfig_la_LIBADD = \
	-lpcre \
	$(NULL)

libeditorconfig_la_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libide/editorconfig/libeditorconfig \
	-D_GNU_SOURCE \
	-DHAVE_STRCASECMP \
	-DHAVE_STRICMP \
	-DHAVE_STRDUP \
	-DHAVE_STRNDUP \
	-DUNIX \
	-Deditorconfig_VERSION_MAJOR=0 \
	-Deditorconfig_VERSION_MINOR=0 \
	-Deditorconfig_VERSION_PATCH=0 \
	-Deditorconfig_VERSION_SUFFIX=0 \
	$(NULL)
