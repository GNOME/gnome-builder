AUTOMAKE_OPTIONS = subdir-objects

include $(srcdir)/m4/filelist.am

noinst_PROGRAMS = extra_defs_gen/generate_defs_libide

extra_defs_gen_generate_defs_libide_SOURCES = extra_defs_gen/generate_defs_libide.cc
extra_defs_gen_generate_defs_libide_LDADD = \
	$(LIBIDEMM_LIBS) -lglibmm_generate_extra_defs-2.4 -L../../libide -lide-1.0

AM_CPPFLAGS = -I$(top_builddir) $(LIBIDE_CFLAGS) $(LIBIDEMM_CFLAGS) -I../../libide -I../../contrib/pnl
