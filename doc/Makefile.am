SUBDIRS = examples

EXTRA_DIST = \
	api/index.rst \
	contributing/code.rst \
	contributing/index.rst \
	contributing/planning.rst \
	contributing/writingdocs.rst \
	credits.rst \
	exploring.rst \
	index.rst \
	installation.rst \
	plugins/building/buildsystem.rst \
	plugins/building/index.rst \
	plugins/building/pipeline.rst \
	plugins/creating.rst \
	plugins/devices.rst \
	plugins/editor/autocompletion.rst \
	plugins/editor/buffers.rst \
	plugins/editor/diagnostics.rst \
	plugins/editor/filesettings.rst \
	plugins/editor/highlighting.rst \
	plugins/editor/index.rst \
	plugins/editor/snippets.rst \
	plugins/greeter.rst \
	plugins/index.rst \
	plugins/keybindings.rst \
	plugins/langserv.rst \
	plugins/menus.rst \
	plugins/preferences.rst \
	plugins/processes/index.rst \
	plugins/processes/runtimes.rst \
	plugins/processes/subprocesses.rst \
	plugins/running.rst \
	plugins/search.rst \
	plugins/symbols/gotodef.rst \
	plugins/symbols/index.rst \
	plugins/symbols/rename.rst \
	plugins/symbols/symbols.rst \
	plugins/transfers.rst \
	plugins/vcs.rst \
	plugins/workbench/actions.rst \
	plugins/workbench/basics.rst \
	plugins/workbench/headerbar.rst \
	plugins/workbench/index.rst \
	plugins/workbench/panels.rst \
	plugins/workbench/perspectives.rst \
	plugins/workbench/widgets.rst \
	plugins/workers.rst \
	projects/building.rst \
	projects/creating.rst \
	projects/debugging.rst \
	projects/index.rst \
	projects/profiling.rst \
	projects/sharing.rst \
	$(NULL)

SPHINX_BUILD = sphinx-build

dist-hook:
	rm -rf html ; $(SPHINX_BUILD) "$(top_srcdir)/doc" "$(distdir)/html"

install-data-hook:
	if test -d "$(srcdir)/html"; then                            \
		$(MKDIR_P) "$(DESTDIR)$(docdir)/html" ;              \
		cp -a $(srcdir)/html/* "$(DESTDIR)$(docdir)/html/" ; \
	fi

uninstall-local:
	rm -rf "$(DESTDIR)$(docdir)/html/"

GITIGNOREFILES = _build

-include $(top_srcdir)/git.mk
