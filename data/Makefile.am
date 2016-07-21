SUBDIRS = gsettings icons style-schemes fonts .

@APPSTREAM_XML_RULES@

appstream_in_files = org.gnome.Builder.appdata.xml.in
appstream_XML = $(appstream_in_files:.xml.in=.xml)

%.appdata.xml: %.appdata.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

# Desktop launcher and description file.
desktopdir = $(datadir)/applications
desktop_in_in_files = org.gnome.Builder.desktop.in.in
desktop_in_files = org.gnome.Builder.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

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
	$(appstream_in_files) \
	$(desktop_in_in_files) \
	$(service_in_files) \
	$(NULL)

CLEANFILES = \
	$(appstream_XML) \
	$(desktop_DATA) \
	$(desktop_in_files) \
	$(service_DATA) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_in_files) \
	$(NULL)

desktop-file-validate: $(desktop_DATA)
	for f in $^; do \
		$(DESKTOP_FILE_VALIDATE) $$f; \
	done

check-local: desktop-file-validate

.PHONY: desktop-file-validate

-include $(top_srcdir)/git.mk
