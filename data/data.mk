data_appdatadir = $(datadir)/appdata
data_appdata_DATA = data/org.gnome.Builder.appdata.xml
EXTRA_DIST += $(data_appdata_DATA)

# Desktop launcher and description file.
data_desktopdir = $(datadir)/applications
data_desktop_DATA = data/org.gnome.Builder.desktop
EXTRA_DIST += $(data_desktop_DATA)

# D-Bus service file.
servicedir = $(datadir)/dbus-1/services
service_in_files = data/org.gnome.Builder.service.in
service_DATA = $(service_in_files:.service.in=.service)
EXTRA_DIST += $(service_in_files)
CLEANFILES += $(service_DATA)

# GtkSourceView Style Scheme
styledir = $(datadir)/gtksourceview-3.0/styles/
style_DATA = \
	data/style-schemes/builder.xml \
	data/style-schemes/builder-dark.xml \
	$(NULL)
EXTRA_DIST += $(style_DATA)

data/org.gnome.Builder.service: data/org.gnome.Builder.service.in
	$(AM_V_GEN)	\
		[ -d $(@D) ] || $(mkdir_p) $(@D) ; \
		sed -e "s|\@bindir\@|$(bindir)|" $< > $@.tmp && mv $@.tmp $@
