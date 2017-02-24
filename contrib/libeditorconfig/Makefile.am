if ENABLE_EDITORCONFIG

noinst_LTLIBRARIES = libeditorconfig.la

libeditorconfig_la_SOURCES =               \
	ec_glob.c                          \
	ec_glob.h                          \
	editorconfig.c                     \
	editorconfig.h                     \
	editorconfig/editorconfig.h        \
	editorconfig/editorconfig_handle.h \
	editorconfig_handle.c              \
	editorconfig_handle.h              \
	global.h                           \
	ini.c                              \
	ini.h                              \
	misc.c                             \
	misc.h                             \
	utarray.h                          \
	$(NULL)

libeditorconfig_la_LIBADD =                \
	-lpcre                             \
	$(NULL)

libeditorconfig_la_CFLAGS =                \
	-D_GNU_SOURCE                      \
	-DHAVE_STRCASECMP                  \
	-DHAVE_STRICMP                     \
	-DHAVE_STRDUP                      \
	-DHAVE_STRNDUP                     \
	-DUNIX                             \
	-Deditorconfig_VERSION_MAJOR=0     \
	-Deditorconfig_VERSION_MINOR=0     \
	-Deditorconfig_VERSION_PATCH=0     \
	-Deditorconfig_VERSION_SUFFIX=0    \
	$(NULL)

endif

-include $(top_srcdir)/git.mk
