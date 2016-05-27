if ENABLE_AUTOTOOLS_TEMPLATES

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = autotools-templates.plugin

moduledir = $(libdir)/gnome-builder/plugins/autotools_templates
dist_module_DATA = autotools_templates/__init__.py

resourcedir = $(datadir)/gnome-builder/plugins/
nobase_resource_DATA = \
	autotools_templates/resources/CONTRIBUTING.md \
	autotools_templates/resources/Makefile.am \
	autotools_templates/resources/NEWS \
	autotools_templates/resources/README.md \
	autotools_templates/resources/autogen.sh \
	autotools_templates/resources/configure.ac \
	autotools_templates/resources/data/Makefile.am \
	autotools_templates/resources/data/package.pc.in \
	autotools_templates/resources/git.mk \
	autotools_templates/resources/m4/Makefile.am \
	autotools_templates/resources/m4/appstream-xml.m4 \
	autotools_templates/resources/m4/ax_append_compile_flags.m4 \
	autotools_templates/resources/m4/ax_append_flag.m4 \
	autotools_templates/resources/m4/ax_check_compile_flag.m4 \
	autotools_templates/resources/m4/ax_check_link_flag.m4 \
	autotools_templates/resources/m4/ax_compiler_flags_cxxflags.m4 \
	autotools_templates/resources/m4/ax_compiler_vendor.m4 \
	autotools_templates/resources/m4/ax_cxx_compile_stdcxx_11.m4 \
	autotools_templates/resources/m4/ax_require_defined.m4 \
	autotools_templates/resources/m4/glib-gettext.m4 \
	autotools_templates/resources/m4/gsettings.m4 \
	autotools_templates/resources/m4/intltool.m4 \
	autotools_templates/resources/m4/introspection.m4 \
	autotools_templates/resources/m4/libtool.m4 \
	autotools_templates/resources/m4/pkg.m4 \
	autotools_templates/resources/m4/vala.m4 \
	autotools_templates/resources/m4/vapigen.m4 \
	autotools_templates/resources/po/LINGUAS \
	autotools_templates/resources/po/Makevars \
	autotools_templates/resources/po/POTFILES.in \
	autotools_templates/resources/src/Makefile.empty \
	autotools_templates/resources/src/Makefile.shared-library-c \
	autotools_templates/resources/src/Makefile.shared-library-vala \
	autotools_templates/resources/src/package-version.h.in \
	autotools_templates/resources/src/package.h \
	autotools_templates/resources/src/package.vala \
	autotools_templates/resources/src/package.deps.in \
	autotools_templates/resources/src/package-namespace.vala.in \
	$(NULL)

EXTRA_DIST = $(nobase_resource_DATA)

endif

-include $(top_srcdir)/git.mk
