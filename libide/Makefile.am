noinst_LTLIBRARIES += libide.la

libide_la_SOURCES = \
	libide/autotools/ide-autotools-build-system.c \
	libide/autotools/ide-autotools-build-system.h \
	libide/autotools/ide-autotools-build-task.c \
	libide/autotools/ide-autotools-build-task.h \
	libide/autotools/ide-autotools-builder.c \
	libide/autotools/ide-autotools-builder.h \
	libide/directory/ide-directory-build-system.c \
	libide/directory/ide-directory-build-system.h \
	libide/directory/ide-directory-vcs.c \
	libide/directory/ide-directory-vcs.h \
	libide/gconstructor.h \
	libide/git/ide-git-vcs.c \
	libide/git/ide-git-vcs.h \
	libide/ide-async-helper.c \
	libide/ide-async-helper.h \
	libide/ide-back-forward-item.c \
	libide/ide-back-forward-item.h \
	libide/ide-back-forward-list.c \
	libide/ide-back-forward-list.h \
	libide/ide-buffer-iter.h \
	libide/ide-buffer.h \
	libide/ide-build-result.c \
	libide/ide-build-result.h \
	libide/ide-build-system.c \
	libide/ide-build-system.h \
	libide/ide-builder.c \
	libide/ide-builder.h \
	libide/ide-context.c \
	libide/ide-context.h \
	libide/ide-debugger.h \
	libide/ide-deployer.h \
	libide/ide-device-manager.c \
	libide/ide-device-manager.h \
	libide/ide-device-provider.c \
	libide/ide-device-provider.h \
	libide/ide-device.c \
	libide/ide-device.h \
	libide/ide-diagnostic-provider.h \
	libide/ide-diagnostic.h \
	libide/ide-executable.h \
	libide/ide-executer.h \
	libide/ide-file.c \
	libide/ide-file.h \
	libide/ide-global.h \
	libide/ide-indenter.h \
	libide/ide-language.c \
	libide/ide-language.h \
	libide/ide-object.c \
	libide/ide-object.h \
	libide/ide-process.h \
	libide/ide-project-file.c \
	libide/ide-project-file.h \
	libide/ide-project-files.c \
	libide/ide-project-files.h \
	libide/ide-project-item.c \
	libide/ide-project-item.h \
	libide/ide-project.c \
	libide/ide-project.h \
	libide/ide-refactory.h \
	libide/ide-script.c \
	libide/ide-script.h \
	libide/ide-source-location.c \
	libide/ide-source-location.h \
	libide/ide-search-engine.h \
	libide/ide-search-provider.h \
	libide/ide-search-result.h \
	libide/ide-service.c \
	libide/ide-service.h \
	libide/ide-symbol-resolver.h \
	libide/ide-symbol.h \
	libide/ide-target.h \
	libide/ide-test-case.h \
	libide/ide-test-suite.h \
	libide/ide-types.h \
	libide/ide-unsaved-files.c \
	libide/ide-unsaved-files.h \
	libide/ide-vcs.c \
	libide/ide-vcs.h \
	libide/ide.c \
	libide/ide.h \
	libide/local/ide-local-device.c \
	libide/local/ide-local-device.h \
	libide/tasks/ide-load-directory-task.c \
	libide/tasks/ide-load-directory-task.h \
	$(NULL)

libide_la_CFLAGS = \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/libide/autotools \
	-I$(top_srcdir)/libide/directory \
	-I$(top_srcdir)/libide/git \
	-I$(top_srcdir)/libide/local \
	-I$(top_srcdir)/libide/tasks \
	$(LIBIDE_CFLAGS)

libide_la_LIBADD = \
	$(LIBIDE_LIBS)
