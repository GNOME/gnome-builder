if ENABLE_IDEMM

include $(srcdir)/m4/filelist.am

dist_noinst_DATA = $(addprefix m4/,$(files_codegen_m4))

noinst_PROGRAMS = extra_defs_gen/generate_defs_libide

extra_defs_gen_generate_defs_libide_SOURCES = extra_defs_gen/generate_defs_libide.cc
extra_defs_gen_generate_defs_libide_LDADD = \
	$(IDEMM_LIBS) \
	$(LIBIDE_LIBS) \
	-lglibmm_generate_extra_defs-2.4 \
	-L$(top_builddir)/libide \
	-lide-1.0

AM_CPPFLAGS = \
	-I$(top_builddir) \
	$(LIBIDE_CFLAGS) \
	$(IDEMM_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/contrib/pnl

endif
