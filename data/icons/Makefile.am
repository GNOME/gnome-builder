SUBDIRS = hicolor

icondir = $(datadir)/icons

nobase_icon_DATA = \
	hicolor/256x256/apps/builder.png \
	hicolor/512x512/apps/builder.png \
	hicolor/16x16/apps/builder.png \
	hicolor/32x32/apps/builder.png \
	hicolor/48x48/apps/builder.png \
	hicolor/24x24/apps/builder.png \
	hicolor/scalable/apps/builder-symbolic.svg \
	$(NULL)

EXTRA_DIST = $(nobase_icon_DATA)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

-include $(top_srcdir)/git.mk
