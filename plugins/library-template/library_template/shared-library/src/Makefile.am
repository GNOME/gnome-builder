lib_LTLIBRARIES = lib{{name}}-@API_VERSION@.la

lib{{name_}}_@API_VERSION@_la_SOURCES = \
	{{prefix}}-version.h \
	{{name}}.h \
	$(NULL)

lib{{name_}}_@API_VERSION@_la_CFLAGS = $\{{{NAME}}_CFLAGS}
lib{{name_}}_@API_VERSION@_la_LIBADD = $\{{{NAME}}_LIBS}

-include $(top_srcdir)/git.mk
