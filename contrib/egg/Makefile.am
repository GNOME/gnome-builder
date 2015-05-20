noinst_LTLIBRARIES = libegg.la

libegg_la_SOURCES = \
	egg-binding-set.c \
	egg-binding-set.h \
	egg-counter.c \
	egg-counter.h \
	egg-heap.c \
	egg-heap.h \
	egg-search-bar.c \
	egg-search-bar.h \
	egg-settings-sandwich.c \
	egg-settings-sandwich.h \
	egg-signal-group.c \
	egg-signal-group.h \
	egg-state-machine.c \
	egg-state-machine.h \
	egg-state-machine-action.c \
	egg-state-machine-action.h \
	egg-state-machine-buildable.c \
	egg-state-machine-buildable.h \
	egg-state-machine-private.h \
	egg-task-cache.c \
	egg-task-cache.h \
	$(NULL)

libegg_la_CFLAGS = $(EGG_CFLAGS)
libegg_la_LIBADD = \
	$(EGG_LIBS) \
	$(SHM_LIB) \
	$(NULL)

-include $(top_srcdir)/git.mk
