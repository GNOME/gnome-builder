CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST =              \
	test-stream-1.txt \
	$(NULL)

pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libmi2-glib.la

libmi2_glib_la_public_sources =          \
	mi2-client.c                     \
	mi2-client.h                     \
	mi2-command-message.c            \
	mi2-command-message.h            \
	mi2-console-message.c            \
	mi2-console-message.h            \
	mi2-event-message.c              \
	mi2-event-message.h              \
	mi2-info-message.c               \
	mi2-info-message.h               \
	mi2-input-stream.c               \
	mi2-input-stream.h               \
	mi2-message.c                    \
	mi2-message.h                    \
	mi2-output-stream.c              \
	mi2-output-stream.h              \
	mi2-util.c                       \
	mi2-util.h                       \
	$(NULL)

libmi2_glib_la_SOURCES =                 \
	$(libmi2_glib_la_public_sources) \
	mi2-glib.h                       \
	$(NULL)

libmi2_glib_la_CFLAGS =                     \
	-DMI2_GLIB_COMPILATION              \
	$(DEBUG_CFLAGS)                     \
	$(MI2_CFLAGS)                       \
	$(OPTIMIZE_CFLAGS)                  \
	$(NULL)

libmi2_glib_la_LIBADD = $(MI2_LIBS)
libmi2_glib_la_LDFLAGS = $(OPTIMIZE_LDFLAGS)

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

Mi2-1.0.gir: libmi2-glib.la
Mi2_1_0_gir_INCLUDES = Gio-2.0
Mi2_1_0_gir_CFLAGS = $(libmi2_glib_la_CFLAGS)
Mi2_1_0_gir_LIBS = libmi2-glib.la
Mi2_1_0_gir_FILES = $(libmi2_glib_la_public_sources)
Mi2_1_0_gir_SCANNERFLAGS =       \
	--c-include="mi2-glib.h" \
	-n Mi2                   \
	$(NULL)
INTROSPECTION_GIRS += Mi2-1.0.gir

girdir = $(datadir)/gnome-builder/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(pkglibdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif

if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

mi2-glib.vapi: Mi2-1.0.gir

VAPIGEN_VAPIS = mi2-glib.vapi

mi2_glib_vapi_DEPS = gio-2.0 json-glib-1.0
mi2_glib_vapi_METADATADIRS = $(srcdir)
mi2_glib_vapi_FILES = Mi2-1.0.gir

mi2-glib.deps: Makefile
	$(AM_V_GEN) echo $(libmi2_glib_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/gnome-builder/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

EXTRA_DIST += mi2-glib.deps

DISTCLEANFILES += $(vapi_DATA)
endif

noinst_PROGRAMS =
TESTS =

tests_cflags =                              \
	-DTEST_DATA_DIR="\"$(abs_srcdir)\"" \
	$(MI2_CFLAGS)                       \
	$(NULL)

tests_libs =           \
	$(MI2_LIBS)    \
	libmi2-glib.la \
	$(NULL)

noinst_PROGRAMS += test-stream
TESTS += test-stream
test_stream_CFLAGS = $(tests_cflags)
test_stream_LDADD = $(tests_libs)

noinst_PROGRAMS += test-client
test_client_CFLAGS = $(tests_cflags)
test_client_LDADD = $(tests_libs)

-include $(top_srcdir)/git.mk
