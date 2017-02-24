pkglibdir = $(libdir)/gnome-builder/
pkglib_LTLIBRARIES = libsearch.la

libsearch_la_SOURCES =      \
	trie.c              \
	trie.h              \
	fuzzy.c             \
	fuzzy.h             \
	$(NULL)

libsearch_la_CFLAGS =       \
	$(DEBUG_CFLAGS)     \
	$(OPTIMIZE_CFLAGS)  \
	$(SEARCH_CFLAGS)    \
	$(NULL)

libsearch_la_LIBADD =       \
	$(SEARCH_LIBADD)    \
	$(NULL)

libsearch_la_LDFLAGS =      \
	$(OPTIMIZE_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
