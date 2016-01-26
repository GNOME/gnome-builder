DISTCLEANFILES =
CLEANFILES =
BUILT_SOURCES =

pkglib_LTLIBRARIES = libtemplate-glib-1.0.la

AM_CPPFLAGS = \
	-D_GNU_SOURCE \
	-DTMPL_GLIB_COMPILATION

EXTRA_DIST = \
	tmpl-expr-parser.c \
	tmpl-expr-parser.h \
	tmpl-expr-scanner.c \
	tmpl-expr-scanner.h

pkgincludedir = $(includedir)/template-glib-1.0
pkginclude_HEADERS = \
	tmpl-error.h \
	tmpl-expr-types.h \
	tmpl-expr.h \
	tmpl-glib.h \
	tmpl-scope.h \
	tmpl-symbol.h \
	tmpl-template-locator.h \
	tmpl-template.h \
	$(NULL)

libtemplate_glib_1_0_la_CFLAGS = $(TMPL_CFLAGS)
libtemplate_glib_1_0_la_LIBADD = $(TMPL_LIBS)
libtemplate_glib_1_0_la_SOURCES = \
	$(pkginclude_HEADERS) \
	tmpl-branch-node.c \
	tmpl-branch-node.h \
	tmpl-condition-node.c \
	tmpl-condition-node.h \
	tmpl-debug.h \
	tmpl-enums.c \
	tmpl-enums.h \
	tmpl-error.c \
	tmpl-expr-eval.c \
	tmpl-expr-node.c \
	tmpl-expr-node.h \
	tmpl-expr-parser-private.h \
	tmpl-expr-parser.y \
	tmpl-expr-private.h \
	tmpl-expr-scanner.l \
	tmpl-expr.c \
	tmpl-gi-private.h \
	tmpl-gi.c \
	tmpl-iter-node.c \
	tmpl-iter-node.h \
	tmpl-iterator.c \
	tmpl-iterator.h \
	tmpl-lexer.c \
	tmpl-lexer.h \
	tmpl-node.c \
	tmpl-node.h \
	tmpl-parser.c \
	tmpl-parser.h \
	tmpl-scope.c \
	tmpl-symbol.c \
	tmpl-template-locator.c \
	tmpl-template.c \
	tmpl-text-node.c \
	tmpl-text-node.h \
	tmpl-token-input-stream.c \
	tmpl-token-input-stream.h \
	tmpl-token.c \
	tmpl-token.h \
	tmpl-util-private.h \
	tmpl-util.c \
	$(NULL)

glib_enum_h = tmpl-enums.h
glib_enum_c = tmpl-enums.c
glib_enum_headers = \
	tmpl-error.h \
	tmpl-expr-types.h \
	$(NULL)
include $(top_srcdir)/build/autotools/Makefile.am.enums

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

introspection_sources = \
	tmpl-enums.c \
	tmpl-enums.h \
	tmpl-error.h \
	tmpl-expr.c \
	tmpl-expr.h \
	tmpl-expr-types.h \
	tmpl-scope.c \
	tmpl-scope.h \
	tmpl-symbol.c \
	tmpl-symbol.h \
	tmpl-template-locator.c \
	tmpl-template-locator.h \
	tmpl-template.c \
	tmpl-template.h \
	$(NULL)

Template-1.0.gir: libtemplate-glib-1.0.la
Template_1_0_gir_INCLUDES = Gio-2.0
Template_1_0_gir_CFLAGS = $(libtemplate_glib_1_0_la_CFLAGS)
Template_1_0_gir_LIBS = libtemplate-glib-1.0.la
Template_1_0_gir_FILES = $(introspection_sources)
Template_1_0_gir_SCANNERFLAGS = \
	-n Template \
	--identifier-prefix Tmpl \
	--symbol-prefix tmpl \
	-DTMPL_GLIB_COMPILATION \
	$(NULL)
INTROSPECTION_GIRS += Template-1.0.gir

pkggirdir = $(datadir)/gnome-builder/gir-1.0
pkggir_DATA = $(INTROSPECTION_GIRS)

pkgtypelibdir = $(libdir)/gnome-builder/girepository-1.0
pkgtypelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

template-glib-1.0.vapi: Template-1.0.gir Template-1.0.metadata

VAPIGEN_VAPIS = template-glib-1.0.vapi

template_glib_1_0_vapi_DEPS = gio-2.0
template_glib_1_0_vapi_METADATADIRS = $(srcdir)
template_glib_1_0_vapi_FILES = Template-1.0.gir

template-glib-1.0.deps: Makefile
	$(AM_V_GEN) echo $(template_glib_1_0_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/gnome-builder/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

EXTRA_DIST += template-glib-1.0.deps

DISTCLEANFILES += $(vapi_DATA)
endif

GITIGNOREFILES = \
	tmpl-expr-parser.c \
	tmpl-expr-parser.h \
	tmpl-expr-scanner.c \
	tmpl-expr-scanner.h \
	tmpl-debug.h \
	*.typelib \
	*.gir \
	lex.*

-include $(top_srcdir)/git.mk
