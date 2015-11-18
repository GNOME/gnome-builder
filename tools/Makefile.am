tools_PROGRAMS =

tools_cflags = \
	-I$(top_srcdir)/libide \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/src/util \
	$(LIBIDE_CFLAGS) \
	$(NULL)

tools_libs = \
	$(LIBIDE_LIBS) \
	$(top_builddir)/libide/libide-1.0.la \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(NULL)

toolsdir = $(libexecdir)/gnome-builder

tools_PROGRAMS += ide-build
ide_build_SOURCES = ide-build.c
ide_build_CFLAGS = $(tools_cflags)
ide_build_LDADD = $(tools_libs)

tools_PROGRAMS += ide-list-build-flags
ide_list_build_flags_SOURCES = ide-list-build-flags.c
ide_list_build_flags_CFLAGS = $(tools_cflags)
ide_list_build_flags_LDADD = $(tools_libs)

tools_PROGRAMS += ide-list-file-settings
ide_list_file_settings_SOURCES = ide-list-file-settings.c
ide_list_file_settings_CFLAGS = $(tools_cflags)
ide_list_file_settings_LDADD = $(tools_libs)

tools_PROGRAMS += ide-list-devices
ide_list_devices_SOURCES = ide-list-devices.c
ide_list_devices_CFLAGS = $(tools_cflags)
ide_list_devices_LDADD = $(tools_libs)

tools_PROGRAMS += ide-list-diagnostics
ide_list_diagnostics_SOURCES = ide-list-diagnostics.c
ide_list_diagnostics_CFLAGS = $(tools_cflags)
ide_list_diagnostics_LDADD = $(tools_libs)

tools_PROGRAMS += ide-list-files
ide_list_files_SOURCES = ide-list-files.c
ide_list_files_CFLAGS = $(tools_cflags)
ide_list_files_LDADD = $(tools_libs)

tools_PROGRAMS += ide-search
ide_search_SOURCES = ide-search.c
ide_search_CFLAGS = $(tools_cflags)
ide_search_LDADD = $(tools_libs)

tools_PROGRAMS += ide-mine-projects
ide_mine_projects_SOURCES = ide-mine-projects.c
ide_mine_projects_CFLAGS = $(tools_cflags)
ide_mine_projects_LDADD = $(tools_libs)

tools_PROGRAMS += ide-list-counters
ide_list_counters_SOURCES = ide-list-counters.c
ide_list_counters_CFLAGS = \
	$(EGG_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	$(NULL)
ide_list_counters_LDADD = \
	$(EGG_LIBS) \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(SHM_LIB) \
	$(NULL)

-include $(top_srcdir)/git.mk
