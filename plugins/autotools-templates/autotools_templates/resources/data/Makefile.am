{{if template == "shared-library"}}
# pkg-config data
# Note that the template file is called {{name}}.pc.in, but generates a
# versioned .pc file using some magic in AC_CONFIG_FILES.
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = {{name}}-$(API_VERSION).pc

DISTCLEANFILES = $(pkgconfig_DATA)
EXTRA_DIST = {{name}}.pc.in
{{end}}

-include $(top_srcdir)/git.mk
