# Rules for generating gresources using glib-compile-resources
#
# Define:
# 	glib_resources_h = header template file
# 	glib_resources_c = source template file
# 	glib_resources_xml = path to *.gresource.xml
# 	glib_resources_namespace = c prefix for resources
#
# before including Makefile.am.resources. You will also need to have
# the following targets already defined:
#
# 	CLEANFILES
#	DISTCLEANFILES
#	BUILT_SOURCES
#	EXTRA_DIST
#
# Author: Christian Hergert <christian@hergert.me>

# Basic sanity checks
$(if $(GLIB_COMPILE_RESOURCES),,$(error Need to define GLIB_COMPILE_RESOURCES))

$(if $(or $(glib_resources_h), \
          $(glib_resources_c)),, \
    $(error Need to define glib_resources_h and glib_resources_c))

$(if $(glib_resources_xml),,$(error Need to define glib_resources_xml))
$(if $(glib_resources_namespace),,$(error Need to define glib_resources_namespace))

resources_xml=$(addprefix $(srcdir)/,$(glib_resources_xml))
resources_srcdir=$(dir $(resources_xml))

DISTCLEANFILES += $(glib_resources_h) $(glib_resources_c)
BUILT_SOURCES += $(glib_resources_h) $(glib_resources_c)
CLEANFILES += stamp-resources $(glib_resources_c) $(glib_resources_h)
EXTRA_DIST += \
	$(glib_resources_xml) \
	$(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(resources_srcdir) --generate-dependencies $(resources_xml)) \
	$(NULL)

stamp-resources: $(glib_resources_c) $(resources_xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) \
		--target=xgen-gr.h \
		--sourcedir=$(resources_srcdir) \
		--generate-header \
		--c-name $(glib_resources_namespace) \
		$(resources_xml) \
	&& (cmp -s xgen-gr.h $(glib_resources_h) || cp -f xgen-gr.h $(glib_resources_h)) \
	&& rm -f xgen-gr.h \
	&& echo timestamp > $(@F)

$(glib_resources_h): stamp-resources
	@true

$(glib_resources_c): $(resources_xml) $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(resources_srcdir) --generate-dependencies $(resources_xml))
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) \
		--target=xgen-gr.c \
		--sourcedir=$(resources_srcdir) \
		--generate-source \
		--c-name $(glib_resources_namespace) \
		$(resources_xml) \
	&& (cmp -s xgen-gr.c $(glib_resources_c) || cp -f xgen-gr.c $(glib_resources_c)) \
	&& rm -f xgen-gr.c
