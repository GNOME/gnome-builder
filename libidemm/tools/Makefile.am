AUTOMAKE_OPTIONS = subdir-objects

include $(srcdir)/m4/filelist.am

noinst_PROGRAMS = extra_defs_gen/generate_defs_gst

extra_defs_gen_generate_plugin_gmmproc_file_SOURCES = \
	extra_defs_gen/generate_plugin_gmmproc_file.cc
extra_defs_gen_generate_plugin_gmmproc_file_LDADD = $(GSTREAMERMM_LIBS)

extra_defs_gen_generate_defs_gst_SOURCES = extra_defs_gen/generate_defs_gst.cc
extra_defs_gen_generate_defs_gst_LDADD = \
	$(LIBIDEMM_LIBS) -lglibmm_generate_extra_defs-2.4

AM_CPPFLAGS = -I$(top_builddir) $(LIBIDEMM_CFLAGS)
AM_CXXFLAGS = $(LIBIDEMM_WXXFLAGS)
