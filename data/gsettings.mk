gsettingsschema_in_files = \
	data/gsettings/org.gnome.builder.editor.gschema.xml.in \
	data/gsettings/org.gnome.builder.editor.language.gschema.xml.in \
	data/gsettings/org.gnome.builder.experimental.gschema.xml.in \
	$(NULL)

gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

@GSETTINGS_RULES@

EXTRA_DIST += \
	$(gsettingsschema_in_files) \
	$(NULL)

CLEANFILES += \
	$(BUILT_SOURCES) \
	$(gsettings_SCHEMAS) \
	$(NULL)

