## Process this file with automake to produce Makefile.in

# see /gtk-doc/examples/Makefile.am

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# The name of the module, e.g. 'glib'.
DOC_MODULE=libide

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
DOC_SOURCE_DIR=$(top_srcdir)/libide

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED"
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

if ENABLE_DOC_CROSS_REFERENCES
# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS = \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib		\
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gmodule		\
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject		\
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gio		\
	--extra-dir=$(GTK_PREFIX)/share/gtk-doc/html/gtk		\
	--extra-dir=$(GTK_PREFIX)/share/gtk-doc/html/gtksourceview-3.0
endif

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB = \
	$(top_srcdir)/libide/ide*.h \
	$(top_srcdir)/libide/*/ide-*.h \
	$(NULL)
CFILE_GLOB = \
	$(top_srcdir)/libide/ide*.c \
	$(top_srcdir)/libide/*/ide-*.c \
	$(NULL)

# Header files to ignore when scanning.
IGNORE_HFILES = 

# Images to copy into HTML directory.
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files = \
	version.xml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
GTKDOC_CFLAGS = \
	-I$(top_srcdir)/libide		\
	-I$(top_builddir)/libide	\
	$(LIBIDE_CFLAGS)

GTKDOC_LIBS = \
	$(top_builddir)/libide/libide-1.0.la		\
	$(LIBIDE_LIBS)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += \
	version.xml.in


if ENABLE_GTK_DOC

TESTS_ENVIRONMENT = \
	SRCDIR=$(abs_srcdir)				\
	BUILDDIR=$(abs_builddir)			\
	DOC_MODULE=$(DOC_MODULE)			\
	DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE)

check-local: $(GTKDOC_CHECK)
	@$(TESTS_ENVIRONMENT) ./$(GTKDOC_CHECK)

endif

-include $(top_srcdir)/git.mk
