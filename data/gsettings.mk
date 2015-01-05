gsettingsschema_in_files = \
	data/org.gnome.builder.editor.gschema.xml.in \
	data/org.gnome.builder.editor.vim.gschema.xml.in \
	data/org.gnome.builder.editor.language.gschema.xml.in

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

