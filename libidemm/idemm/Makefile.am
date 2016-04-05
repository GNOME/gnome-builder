lib_LTLIBRARIES = libidemm-1.0.la
include $(srcdir)/../src/filelist.am
include $(srcdir)/filelist.am
include $(top_srcdir)/libidemm/compile-binding.am

local_cppflags = $(binding_includes) $(binding_cppflags) -DLIBIDEMM_BUILD=1
libide_cflags = $(LIBIDE_CFLAGS) -I$(top_srcdir)/libide -I$(top_srcdir)/contrib/pnl

AM_CPPFLAGS = $(local_cppflags) $(IDEMM_CFLAGS) $(libide_cflags)

AM_CXXFLAGS = $(IDEMM_WXXFLAGS)

libidemm_1_0_la_SOURCES = $(binding_sources)
libidemm_1_0_la_LDFLAGS = -no-undefined -version-info $(IDEMM_SO_VERSION)
libidemm_1_0_la_LIBADD  = $(IDEMM_LIBS)
