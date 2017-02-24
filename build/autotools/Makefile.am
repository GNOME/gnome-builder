EXTRA_DIST =                                         \
	Makefile.am.enums                            \
	Makefile.am.gresources                       \
	Makefile.am.marshal                          \
	appstream-xml.m4                             \
	ax_append_compile_flags.m4                   \
	ax_append_flag.m4                            \
	ax_check_compile_flag.m4                     \
	ax_check_link_flag.m4                        \
	ax_compiler_vendor.m4                        \
	ax_cxx_compile_stdcxx.m4                     \
	ax_cxx_compile_stdcxx_11.m4                  \
	ax_require_defined.m4                        \
	gb_enable_rdtscp.m4                          \
	introspection.m4                             \
	pkg.m4                                       \
	vala.m4                                      \
	vapigen.m4                                   \
	yelp.m4                                      \
	tap-test                                     \
	$(NULL)

# We want to ignore all m4 files except a few
# we explicitly keep in repo
GITIGNOREFILES =                                     \
	$(filter-out $(EXTRA_DIST),$(wildcard *.m4)) \
	$(NULL)

-include $(top_srcdir)/git.mk
