
icondir = $(datadir)/icons

nobase_icon_DATA = \
	hicolor/256x256/apps/builder.png \
	hicolor/512x512/apps/builder.png \
	hicolor/16x16/apps/builder.png \
	hicolor/32x32/apps/builder.png \
	hicolor/48x48/apps/builder.png \
	hicolor/24x24/apps/builder.png \
	hicolor/scalable/actions/builder-split-tab-symbolic.svg \
	hicolor/scalable/apps/builder-documentation-symbolic.svg \
	hicolor/scalable/apps/builder-editor-symbolic.svg \
	hicolor/scalable/autocomplete/lang-enum-value-symbolic.svg \
	hicolor/scalable/autocomplete/struct-field-symbolic.svg \
	hicolor/scalable/autocomplete/lang-class-symbolic.svg \
	hicolor/scalable/autocomplete/lang-struct-symbolic.svg \
	hicolor/scalable/autocomplete/lang-enum-symbolic.svg \
	hicolor/scalable/autocomplete/lang-method-symbolic.svg \
	hicolor/scalable/autocomplete/lang-function-symbolic.svg \
	$(NULL)

bundled_icon_sources = \
	hicolor/scalable/vcs/builder-vcs-git-symbolic.svg \
	hicolor/256x256/vcs/builder-vcs-git.png \
	hicolor/48x48/vcs/builder-vcs-git.png \
	hicolor/32x32/vcs/builder-vcs-git.png \
	$(NULL)

EXTRA_DIST = \
	$(bundled_icon_sources) \
	$(nobase_icon_DATA) \
	hicolor/source.svg

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
