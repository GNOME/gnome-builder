include build/autotools/automake/glib-tap.mk
include build/autotools/automake/Defaults.mk
include build/autotools/automake/ChangeLog.mk
include build/autotools/automake/AutomakeDocs.mk

ACLOCAL_AMFLAGS = -I build/autotools/m4 ${ACLOCAL_FLAGS}

include src/gnome-builder.mk
include data/data.mk
include tests/tests.mk

# XXX: Not happy about this, need to come up with a good way to do
# icon installation that doesn't require subdirs.
SUBDIRS = . data/icons

EXTRA_DIST += \
	tap-driver.sh \
	tap-test
