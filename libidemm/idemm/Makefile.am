if ENABLE_IDEMM

lib_LTLIBRARIES = libidemm-1.0.la
include $(srcdir)/../src/filelist.am
include $(srcdir)/filelist.am
include $(top_srcdir)/libidemm/compile-binding.am

local_cppflags = $(binding_includes) $(binding_cppflags) -DLIBIDEMM_BUILD=1
libide_cflags = $(LIBIDE_CFLAGS) -I$(top_srcdir)/libide -I$(top_srcdir)/contrib/pnl

AM_CPPFLAGS = $(local_cppflags) $(IDEMM_CFLAGS) $(libide_cflags)

AM_CXXFLAGS = $(IDEMM_WXXFLAGS)

libidemm_1_0_la_SOURCES = $(binding_sources)
libidemm_1_0_la_LDFLAGS =
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-no-undefined \
	$(NULL)

libidemm_1_0_la_LIBADD  = $(IDEMM_LIBS)

endif
