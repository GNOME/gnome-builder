SUBDIRS = gsettings icons style-schemes fonts .

appdatadir = $(datadir)/appdata
appdata_DATA = org.gnome.Builder.appdata.xml

# Desktop launcher and description file.
desktopdir = $(datadir)/applications
desktop_in_in_files = org.gnome.Builder.desktop.in.in
desktop_in_files = org.gnome.Builder.desktop.inn
desktop_DATA = org.gnome.Builder.desktop
@INTLTOOL_DESKTOP_RULE@

# D-Bus service file.
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Builder.service.in
service_DATA = $(service_in_files:.service.in=.service)

# pkg-config
pkgconfigdir = $(libdir)/gnome-builder/pkgconfig
pkgconfig_DATA = \
	libide-1.0.pc \
	template-glib-1.0.pc

org.gnome.Builder.service: org.gnome.Builder.service.in
	$(AM_V_GEN)	\
		[ -d $(@D) ] || $(mkdir_p) $(@D) ; \
		sed -e "s|\@bindir\@|$(bindir)|" $< > $@.tmp && mv $@.tmp $@

EXTRA_DIST = \
	$(appdata_DATA) \
	$(desktop_in_in_files) \
	$(service_in_files) \
	$(NULL)

CLEANFILES = \
	$(service_DATA) \
	$(desktop_DATA) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA) \
	$(NULL)

desktop-file-validate: $(desktop_DATA)
	for f in $^; do \
		$(DESKTOP_FILE_VALIDATE) $$f; \
	done

check-local: desktop-file-validate

.PHONY: desktop-file-validate

-include $(top_srcdir)/git.mk
