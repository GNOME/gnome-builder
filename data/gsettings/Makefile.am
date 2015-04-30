gsettings_SCHEMAS = \
	org.gnome.builder.gschema.xml \
	org.gnome.builder.editor.gschema.xml \
	org.gnome.builder.editor.language.gschema.xml \
	org.gnome.builder.experimental.gschema.xml \
	org.gnome.builder.project-tree.gschema.xml \
	$(NULL)

.PRECIOUS: $(gsettings_SCHEMAS)

@GSETTINGS_RULES@

EXTRA_DIST = $(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
