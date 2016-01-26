if ENABLE_LIBRARY_TEMPLATE_PLUGIN

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = library-template.plugin

moduledir = $(libdir)/gnome-builder/plugins/library_template
dist_module_DATA = library_template/__init__.py

resourcedir = $(datadir)/gnome-builder/plugins/
nobase_resource_DATA = \
	library_template/shared-library/README.md \
	library_template/shared-library/src/package.h \
	library_template/shared-library/src/Makefile.am \
	library_template/shared-library/src/package-version.h.in \
	library_template/shared-library/po/POTFILES.in \
	library_template/shared-library/m4/gsettings.m4 \
	library_template/shared-library/m4/ax_require_defined.m4 \
	library_template/shared-library/m4/ax_compiler_vendor.m4 \
	library_template/shared-library/m4/glib-gettext.m4 \
	library_template/shared-library/m4/ax_append_compile_flags.m4 \
	library_template/shared-library/m4/appstream-xml.m4 \
	library_template/shared-library/m4/ax_check_link_flag.m4 \
	library_template/shared-library/m4/ax_check_compile_flag.m4 \
	library_template/shared-library/m4/ax_cxx_compile_stdcxx_11.m4 \
	library_template/shared-library/m4/Makefile.am \
	library_template/shared-library/m4/intltool.m4 \
	library_template/shared-library/m4/ax_append_flag.m4 \
	library_template/shared-library/m4/libtool.m4 \
	library_template/shared-library/m4/vala.m4 \
	library_template/shared-library/m4/introspection.m4 \
	library_template/shared-library/m4/pkg.m4 \
	library_template/shared-library/m4/vapigen.m4 \
	library_template/shared-library/Makefile.am \
	library_template/shared-library/CONTRIBUTING.md \
	library_template/shared-library/git.mk \
	library_template/shared-library/NEWS \
	library_template/shared-library/configure.ac \
	library_template/shared-library/autogen.sh \
	library_template/shared-library/data/package.pc.in \
	library_template/shared-library/data/Makefile.am \
	$(NULL)

EXTRA_DIST = $(nobase_resource_DATA)

endif

GITIGNOREFILES = library_template/__pycache__

-include $(top_srcdir)/git.mk
