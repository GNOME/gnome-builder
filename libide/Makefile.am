noinst_LTLIBRARIES += libide.la

libide_la_public_sources = \
	libide/autotools/ide-autotools-build-system.c \
	libide/autotools/ide-autotools-build-system.h \
	libide/autotools/ide-autotools-build-task.c \
	libide/autotools/ide-autotools-build-task.h \
	libide/autotools/ide-autotools-builder.c \
	libide/autotools/ide-autotools-builder.h \
	libide/c/ide-c-indenter.c \
	libide/c/ide-c-indenter.h \
	libide/c/ide-c-language.c \
	libide/c/ide-c-language.h \
	libide/clang/ide-clang-diagnostic-provider.c \
	libide/clang/ide-clang-diagnostic-provider.h \
	libide/clang/ide-clang-highlighter.c \
	libide/clang/ide-clang-highlighter.h \
	libide/clang/ide-clang-private.h \
	libide/clang/ide-clang-service.c \
	libide/clang/ide-clang-service.h \
	libide/clang/ide-clang-symbol-resolver.c \
	libide/clang/ide-clang-symbol-resolver.h \
	libide/clang/ide-clang-translation-unit.c \
	libide/clang/ide-clang-translation-unit.h \
	libide/directory/ide-directory-build-system.c \
	libide/directory/ide-directory-build-system.h \
	libide/directory/ide-directory-vcs.c \
	libide/directory/ide-directory-vcs.h \
	libide/git/ide-git-vcs.c \
	libide/git/ide-git-vcs.h \
	libide/ide-back-forward-item.c \
	libide/ide-back-forward-item.h \
	libide/ide-back-forward-list.c \
	libide/ide-back-forward-list.h \
	libide/ide-build-result.c \
	libide/ide-build-result.h \
	libide/ide-build-system.c \
	libide/ide-build-system.h \
	libide/ide-builder.c \
	libide/ide-builder.h \
	libide/ide-context.c \
	libide/ide-context.h \
	libide/ide-debugger.c \
	libide/ide-debugger.h \
	libide/ide-deployer.c \
	libide/ide-deployer.h \
	libide/ide-device-manager.c \
	libide/ide-device-manager.h \
	libide/ide-device-provider.c \
	libide/ide-device-provider.h \
	libide/ide-device.c \
	libide/ide-device.h \
	libide/ide-diagnostic-provider.c \
	libide/ide-diagnostic-provider.h \
	libide/ide-diagnostic.c \
	libide/ide-diagnostic.h \
	libide/ide-diagnostician.c \
	libide/ide-diagnostician.h \
	libide/ide-diagnostics.c \
	libide/ide-diagnostics.h \
	libide/ide-executable.c \
	libide/ide-executable.h \
	libide/ide-executer.c \
	libide/ide-executer.h \
	libide/ide-file.c \
	libide/ide-file.h \
	libide/ide-global.h \
	libide/ide-highlighter.c \
	libide/ide-highlighter.h \
	libide/ide-indenter.c \
	libide/ide-indenter.h \
	libide/ide-indenter.h \
	libide/ide-language.c \
	libide/ide-language.h \
	libide/ide-object.c \
	libide/ide-object.h \
	libide/ide-process.c \
	libide/ide-process.h \
	libide/ide-project-file.c \
	libide/ide-project-file.h \
	libide/ide-project-files.c \
	libide/ide-project-files.h \
	libide/ide-project-item.c \
	libide/ide-project-item.h \
	libide/ide-project.c \
	libide/ide-project.h \
	libide/ide-refactory.c \
	libide/ide-refactory.h \
	libide/ide-script.c \
	libide/ide-script.h \
	libide/ide-search-context.c \
	libide/ide-search-context.h \
	libide/ide-search-engine.c \
	libide/ide-search-engine.h \
	libide/ide-search-provider.c \
	libide/ide-search-provider.h \
	libide/ide-search-result.c \
	libide/ide-search-result.h \
	libide/ide-service.c \
	libide/ide-service.h \
	libide/ide-source-location.c \
	libide/ide-source-location.h \
	libide/ide-source-range.c \
	libide/ide-source-range.h \
	libide/ide-symbol-resolver.c \
	libide/ide-symbol-resolver.h \
	libide/ide-symbol.c \
	libide/ide-symbol.h \
	libide/ide-target.c \
	libide/ide-target.h \
	libide/ide-test-case.c \
	libide/ide-test-case.h \
	libide/ide-test-suite.c \
	libide/ide-test-suite.h \
	libide/ide-types.h \
	libide/ide-unsaved-file.c \
	libide/ide-unsaved-file.h \
	libide/ide-unsaved-files.c \
	libide/ide-unsaved-files.h \
	libide/ide-vcs.c \
	libide/ide-vcs.h \
	libide/ide.c \
	libide/ide.h \
	libide/local/ide-local-device.c \
	libide/local/ide-local-device.h \
	$(NULL)

libide_la_SOURCES = \
	$(libide_la_public_sources) \
	libide/gconstructor.h \
	libide/ide-async-helper.c \
	libide/ide-async-helper.h \
	libide/ide-internal.h \
	libide/tasks/ide-load-directory-task.c \
	libide/tasks/ide-load-directory-task.h \
	$(NULL)

libide_la_CFLAGS = \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/libide/autotools \
	-I$(top_srcdir)/libide/c \
	-I$(top_srcdir)/libide/clang \
	-I$(top_srcdir)/libide/directory \
	-I$(top_srcdir)/libide/git \
	-I$(top_srcdir)/libide/local \
	-I$(top_srcdir)/libide/tasks \
	$(LIBIDE_CFLAGS)

libide_la_LIBADD = \
	$(CLANG_LDFLAGS) \
	$(LIBIDE_LIBS) \
	-lclang \
	$(NULL)


-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(top_srcdir)/libide --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_srcdir)/libide

if HAVE_INTROSPECTION
introspection_sources = $(libide_la_public_sources)

Ide.gir: libide.la
Ide_gir_INCLUDES = Gio-2.0 GtkSource-3.0 Ggit-1.0
Ide_gir_CFLAGS = $(libide_la_CFLAGS)
Ide_gir_LIBS = libide.la
Ide_gir_FILES = $(introspection_sources)
Ide_gir_SCANNERFLAGS = \
	--c-include="ide.h" \
	-n Ide \
	$(NULL)
INTROSPECTION_GIRS += Ide.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
