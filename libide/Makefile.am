CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST =

pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libide-1.0.la

headersdir = $(includedir)/gnome-builder-@VERSION@/libide
nobase_headers_DATA =                                                       \
	$(libide_1_0_la_public_headers)                                     \
	$(builddir)/ide-enums.h                                             \
        $(NULL)

libide_1_0_la_public_headers =                                              \
	ide-context.h                                                       \
	ide-debug.h                                                         \
	ide-global.h                                                        \
	ide-macros.h                                                        \
	ide-object.h                                                        \
	ide-service.h                                                       \
	ide-types.h                                                         \
	ide.h                                                               \
	application/ide-application-addin.h                                 \
	application/ide-application-credits.h                               \
	application/ide-application-tool.h                                  \
	application/ide-application.h                                       \
	buffers/ide-buffer-change-monitor.h                                 \
	buffers/ide-buffer-manager.h                                        \
	buffers/ide-buffer.h                                                \
	buffers/ide-unsaved-file.h                                          \
	buffers/ide-unsaved-files.h                                         \
	buildconfig/ide-buildconfig-configuration.h                         \
	buildconfig/ide-buildconfig-configuration-provider.h                \
	buildsystem/ide-build-log.h                                         \
	buildsystem/ide-build-manager.h                                     \
	buildsystem/ide-build-pipeline.h                                    \
	buildsystem/ide-build-pipeline-addin.h                              \
	buildsystem/ide-build-stage.h                                       \
	buildsystem/ide-build-stage-launcher.h                              \
	buildsystem/ide-build-stage-mkdirs.h                                \
	buildsystem/ide-build-stage-transfer.h                              \
	buildsystem/ide-build-system.h                                      \
	buildsystem/ide-build-system-discovery.h                            \
	buildsystem/ide-build-target.h                                      \
	buildsystem/ide-build-utils.h                                       \
	buildsystem/ide-configuration-manager.h                             \
	buildsystem/ide-configuration.h                                     \
	buildsystem/ide-configuration-provider.h                            \
	buildsystem/ide-environment-variable.h                              \
	buildsystem/ide-environment.h                                       \
	debugger/ide-debug-manager.h                                        \
	debugger/ide-debugger-breakpoints.h                                 \
	debugger/ide-debugger-editor-view-addin.h                           \
	debugger/ide-debugger-perspective.h                                 \
	debugger/ide-debugger-workbench-addin.h                             \
	debugger/ide-debugger-gutter-renderer.h                             \
	devices/ide-device-manager.h                                        \
	devices/ide-device-provider.h                                       \
	devices/ide-device.h                                                \
	diagnostics/ide-diagnostic-provider.h                               \
	diagnostics/ide-diagnostic.h                                        \
	diagnostics/ide-diagnostics-manager.h                               \
	diagnostics/ide-diagnostics.h                                       \
	diagnostics/ide-fixit.h                                             \
	diagnostics/ide-source-location.h                                   \
	diagnostics/ide-source-range.h                                      \
	directory/ide-directory-build-system.h                              \
	directory/ide-directory-vcs.h                                       \
	doap/ide-doap-person.h                                              \
	doap/ide-doap.h                                                     \
	editor/ide-editor-perspective.h                                     \
	editor/ide-editor-view-addin.h                                      \
	editor/ide-editor-view.h                                            \
	files/ide-file-settings.defs                                        \
	files/ide-file-settings.h                                           \
	files/ide-file.h                                                    \
	files/ide-indent-style.h                                            \
	formatting/ide-formatter.h                                          \
	formatting/ide-formatter-options.h                                  \
	genesis/ide-genesis-addin.h                                         \
	highlighting/ide-highlight-engine.h                                 \
	highlighting/ide-highlight-index.h                                  \
	highlighting/ide-highlighter.h                                      \
	history/ide-back-forward-item.h                                     \
	history/ide-back-forward-list.h                                     \
	langserv/ide-langserv-client.h                                      \
	langserv/ide-langserv-completion-provider.h                         \
	langserv/ide-langserv-diagnostic-provider.h                         \
	langserv/ide-langserv-formatter.h                                   \
	langserv/ide-langserv-highlighter.h                                 \
	langserv/ide-langserv-rename-provider.h                             \
	langserv/ide-langserv-symbol-node.h                                 \
	langserv/ide-langserv-symbol-resolver.h                             \
	langserv/ide-langserv-symbol-tree.h                                 \
	langserv/ide-langserv-util.h                                        \
	local/ide-local-device.h                                            \
	logging/ide-log.h                                                   \
	plugins/ide-extension-adapter.h                                     \
	plugins/ide-extension-set-adapter.h                                 \
	preferences/ide-preferences-addin.h                                 \
	preferences/ide-preferences-bin.h                                   \
	preferences/ide-preferences-entry.h                                 \
	preferences/ide-preferences-file-chooser-button.h                   \
	preferences/ide-preferences-flow-box.h                              \
	preferences/ide-preferences-font-button.h                           \
	preferences/ide-preferences-group.h                                 \
	preferences/ide-preferences-page.h                                  \
	preferences/ide-preferences-spin-button.h                           \
	preferences/ide-preferences-switch.h                                \
	preferences/ide-preferences.h                                       \
	projects/ide-project-edit.h                                         \
	projects/ide-project-file.h                                         \
	projects/ide-project-files.h                                        \
	projects/ide-project-info.h                                         \
	projects/ide-project-item.h                                         \
	projects/ide-project-miner.h                                        \
	projects/ide-project.h                                              \
	projects/ide-recent-projects.h                                      \
	rename/ide-rename-provider.h                                        \
	runner/ide-run-button.h                                             \
	runner/ide-run-manager.h                                            \
	runner/ide-runner.h                                                 \
	runner/ide-runner-addin.h                                           \
	runtimes/ide-runtime-manager.h                                      \
	runtimes/ide-runtime-provider.h                                     \
	runtimes/ide-runtime.h                                              \
	search/ide-omni-search-display.h                                    \
	search/ide-omni-search-entry.h                                      \
	search/ide-omni-search-group.h                                      \
	search/ide-omni-search-row.h                                        \
	search/ide-pattern-spec.h                                           \
	search/ide-search-context.h                                         \
	search/ide-search-engine.h                                          \
	search/ide-search-provider.h                                        \
	search/ide-search-reducer.h                                         \
	search/ide-search-result.h                                          \
	snippets/ide-source-snippet-chunk.h                                 \
	snippets/ide-source-snippet-context.h                               \
	snippets/ide-source-snippet.h                                       \
	snippets/ide-source-snippets-manager.h                              \
	snippets/ide-source-snippets.h                                      \
	sourceview/ide-completion-item.h                                    \
	sourceview/ide-completion-provider.h                                \
	sourceview/ide-completion-results.h                                 \
	sourceview/ide-completion-words.h                                   \
	sourceview/ide-indenter.h                                           \
	sourceview/ide-language.h                                           \
	sourceview/ide-source-map.h                                         \
	sourceview/ide-source-style-scheme.h                                \
	sourceview/ide-source-view-mode.h                                   \
	sourceview/ide-source-view.h                                        \
	subprocess/ide-subprocess.h                                         \
	subprocess/ide-subprocess-launcher.h                                \
	subprocess/ide-subprocess-supervisor.h                              \
	symbols/ide-symbol-node.h                                           \
	symbols/ide-symbol-resolver.h                                       \
	symbols/ide-symbol-tree.h                                           \
	symbols/ide-symbol.h                                                \
	symbols/ide-tags-builder.h                                          \
	template/ide-project-template.h                                     \
	template/ide-template-base.h                                        \
	template/ide-template-provider.h                                    \
	threading/ide-thread-pool.h                                         \
	transfers/ide-pkcon-transfer.h                                      \
	transfers/ide-transfer-manager.h                                    \
	transfers/ide-transfer-row.h                                        \
	transfers/ide-transfer.h                                            \
	transfers/ide-transfer-button.h                                     \
	transfers/ide-transfers-button.h                                    \
	transfers/ide-transfers-progress-icon.h                             \
	tree/ide-tree-builder.h                                             \
	tree/ide-tree-node.h                                                \
	tree/ide-tree-types.h                                               \
	tree/ide-tree.h                                                     \
	util/ide-cairo.h                                                    \
	util/ide-dnd.h                                                      \
	util/ide-directory-reaper.h                                         \
	util/ide-file-manager.h                                             \
	util/ide-flatpak.h                                                  \
	util/ide-glib.h                                                     \
	util/ide-gtk.h                                                      \
	util/ide-line-reader.h                                              \
	util/ide-list-inline.h                                              \
	util/ide-pango.h                                                    \
	util/ide-posix.h                                                    \
	util/ide-progress.h                                                 \
	util/ide-rgba.h                                                     \
	util/ide-settings.h                                                 \
	util/ide-uri.h                                                      \
	vcs/ide-vcs-config.h                                                \
	vcs/ide-vcs-initializer.h                                           \
	vcs/ide-vcs-uri.h                                                   \
	vcs/ide-vcs.h                                                       \
	workbench/ide-layout-grid.h                                         \
	workbench/ide-layout-pane.h                                         \
	workbench/ide-layout-stack-split.h                                  \
	workbench/ide-layout-stack.h                                        \
	workbench/ide-layout-stack-addin.h                                  \
	workbench/ide-layout-view.h                                         \
	workbench/ide-layout.h                                              \
	workbench/ide-omni-bar.h                                            \
	workbench/ide-perspective.h                                         \
	workbench/ide-workbench-addin.h                                     \
	workbench/ide-workbench-message.h                                   \
	workbench/ide-workbench-header-bar.h                                \
	workbench/ide-workbench.h                                           \
	workers/ide-worker.h                                                \
	$(NULL)

libide_1_0_la_public_sources =                                              \
	application/ide-application-addin.c                                 \
	application/ide-application-tool.c                                  \
	application/ide-application.c                                       \
	application/ide-application-open.c                                  \
	buffers/ide-buffer-change-monitor.c                                 \
	buffers/ide-buffer-manager.c                                        \
	buffers/ide-buffer.c                                                \
	buffers/ide-unsaved-file.c                                          \
	buffers/ide-unsaved-files.c                                         \
	buildconfig/ide-buildconfig-configuration.c                         \
	buildconfig/ide-buildconfig-configuration-provider.c                \
	buildsystem/ide-build-manager.c                                     \
	buildsystem/ide-build-pipeline.c                                    \
	buildsystem/ide-build-pipeline-addin.c                              \
	buildsystem/ide-build-stage.c                                       \
	buildsystem/ide-build-stage-launcher.c                              \
	buildsystem/ide-build-stage-mkdirs.c                                \
	buildsystem/ide-build-stage-transfer.c                              \
	buildsystem/ide-build-system.c                                      \
	buildsystem/ide-build-system-discovery.c                            \
	buildsystem/ide-build-target.c                                      \
	buildsystem/ide-build-utils.c                                       \
	buildsystem/ide-configuration-manager.c                             \
	buildsystem/ide-configuration.c                                     \
	buildsystem/ide-configuration-provider.c                            \
	buildsystem/ide-environment-variable.c                              \
	buildsystem/ide-environment.c                                       \
	debugger/ide-debug-manager.c                                        \
	debugger/ide-debugger-breakpoints.c                                 \
	debugger/ide-debugger-editor-view-addin.c                           \
	debugger/ide-debugger-perspective.c                                 \
	debugger/ide-debugger-workbench-addin.c                             \
	debugger/ide-debugger-gutter-renderer.c                             \
	devices/ide-device-manager.c                                        \
	devices/ide-device-provider.c                                       \
	devices/ide-device.c                                                \
	diagnostics/ide-diagnostic-provider.c                               \
	diagnostics/ide-diagnostic.c                                        \
	diagnostics/ide-diagnostics-manager.c                               \
	diagnostics/ide-diagnostics.c                                       \
	diagnostics/ide-fixit.c                                             \
	diagnostics/ide-source-location.c                                   \
	diagnostics/ide-source-range.c                                      \
	directory/ide-directory-build-system.c                              \
	directory/ide-directory-plugin.c                                    \
	directory/ide-directory-vcs.c                                       \
	doap/ide-doap-person.c                                              \
	doap/ide-doap.c                                                     \
	editor/ide-editor-perspective.c                                     \
	editor/ide-editor-view-addin.c                                      \
	editor/ide-editor-view.c                                            \
	files/ide-file-settings.c                                           \
	files/ide-file-settings.defs                                        \
	files/ide-file.c                                                    \
	formatting/ide-formatter.c                                          \
	formatting/ide-formatter-options.c                                  \
	genesis/ide-genesis-addin.c                                         \
	highlighting/ide-highlight-engine.c                                 \
	highlighting/ide-highlight-index.c                                  \
	highlighting/ide-highlighter.c                                      \
	history/ide-back-forward-item.c                                     \
	history/ide-back-forward-list-load.c                                \
	history/ide-back-forward-list-save.c                                \
	history/ide-back-forward-list.c                                     \
	ide-context.c                                                       \
	ide-object.c                                                        \
	ide-service.c                                                       \
	ide.c                                                               \
	langserv/ide-langserv-client.c                                      \
	langserv/ide-langserv-completion-provider.c                         \
	langserv/ide-langserv-diagnostic-provider.c                         \
	langserv/ide-langserv-formatter.c                                   \
	langserv/ide-langserv-highlighter.c                                 \
	langserv/ide-langserv-rename-provider.c                             \
	langserv/ide-langserv-symbol-node.c                                 \
	langserv/ide-langserv-symbol-node-private.h                         \
	langserv/ide-langserv-symbol-resolver.c                             \
	langserv/ide-langserv-symbol-tree.c                                 \
	langserv/ide-langserv-symbol-tree-private.h                         \
	langserv/ide-langserv-util.c                                        \
	local/ide-local-device.c                                            \
	logging/ide-log.c                                                   \
	plugins/ide-extension-adapter.c                                     \
	plugins/ide-extension-set-adapter.c                                 \
	preferences/ide-preferences-addin.c                                 \
	preferences/ide-preferences-entry.c                                 \
	preferences/ide-preferences-file-chooser-button.c                   \
	preferences/ide-preferences-flow-box.c                              \
	preferences/ide-preferences-font-button.c                           \
	preferences/ide-preferences-group.c                                 \
	preferences/ide-preferences-page.c                                  \
	preferences/ide-preferences-spin-button.c                           \
	preferences/ide-preferences-switch.c                                \
	preferences/ide-preferences.c                                       \
	projects/ide-project-edit.c                                         \
	projects/ide-project-file.c                                         \
	projects/ide-project-files.c                                        \
	projects/ide-project-info.c                                         \
	projects/ide-project-item.c                                         \
	projects/ide-project-miner.c                                        \
	projects/ide-project.c                                              \
	projects/ide-recent-projects.c                                      \
	rename/ide-rename-provider.c                                        \
	runner/ide-run-button.c                                             \
	runner/ide-run-manager.c                                            \
	runner/ide-runner.c                                                 \
	runner/ide-runner-addin.c                                           \
	runtimes/ide-runtime-manager.c                                      \
	runtimes/ide-runtime-provider.c                                     \
	runtimes/ide-runtime.c                                              \
	search/ide-omni-search-display.c                                    \
	search/ide-omni-search-entry.c                                      \
	search/ide-omni-search-group.c                                      \
	search/ide-omni-search-row.c                                        \
	search/ide-pattern-spec.c                                           \
	search/ide-search-context.c                                         \
	search/ide-search-engine.c                                          \
	search/ide-search-provider.c                                        \
	search/ide-search-result.c                                          \
	snippets/ide-source-snippet-chunk.c                                 \
	snippets/ide-source-snippet-context.c                               \
	snippets/ide-source-snippet.c                                       \
	snippets/ide-source-snippets-manager.c                              \
	snippets/ide-source-snippets.c                                      \
	sourceview/ide-completion-item.c                                    \
	sourceview/ide-completion-provider.c                                \
	sourceview/ide-completion-results.c                                 \
	sourceview/ide-completion-words.c                                   \
	sourceview/ide-indenter.c                                           \
	sourceview/ide-language.c                                           \
	sourceview/ide-source-map.c                                         \
	sourceview/ide-source-style-scheme.c                                \
	sourceview/ide-source-view-mode.c                                   \
	sourceview/ide-source-view.c                                        \
	subprocess/ide-subprocess.c                                         \
	subprocess/ide-subprocess-launcher.c                                \
	subprocess/ide-subprocess-supervisor.c                              \
	symbols/ide-symbol-node.c                                           \
	symbols/ide-symbol-resolver.c                                       \
	symbols/ide-symbol-tree.c                                           \
	symbols/ide-symbol.c                                                \
	symbols/ide-tags-builder.c                                          \
	template/ide-project-template.c                                     \
	template/ide-template-base.c                                        \
	template/ide-template-provider.c                                    \
	threading/ide-thread-pool.c                                         \
	transfers/ide-pkcon-transfer.c                                      \
	transfers/ide-transfer-manager.c                                    \
	transfers/ide-transfer-row.c                                        \
	transfers/ide-transfer.c                                            \
	transfers/ide-transfer-button.c                                     \
	transfers/ide-transfers-button.c                                    \
	transfers/ide-transfers-progress-icon.c                             \
	tree/ide-tree-builder.c                                             \
	tree/ide-tree-node.c                                                \
	tree/ide-tree.c                                                     \
	util/ide-cairo.c                                                    \
	util/ide-dnd.c                                                      \
	util/ide-directory-reaper.c                                         \
	util/ide-file-manager.c                                             \
	util/ide-flatpak.c                                                  \
	util/ide-glib.c                                                     \
	util/ide-gtk.c                                                      \
	util/ide-line-reader.c                                              \
	util/ide-pango.c                                                    \
	util/ide-posix.c                                                    \
	util/ide-progress.c                                                 \
	util/ide-rgba.c                                                     \
	util/ide-settings.c                                                 \
	util/ide-uri.c                                                      \
	vcs/ide-vcs-config.c                                                \
	vcs/ide-vcs-initializer.c                                           \
	vcs/ide-vcs-uri.c                                                   \
	vcs/ide-vcs.c                                                       \
	workbench/ide-layout-grid.c                                         \
	workbench/ide-layout-pane.c                                         \
	workbench/ide-layout-stack.c                                        \
	workbench/ide-layout-stack-addin.c                                  \
	workbench/ide-layout-view.c                                         \
	workbench/ide-layout.c                                              \
	workbench/ide-omni-bar.c                                            \
	workbench/ide-perspective.c                                         \
	workbench/ide-workbench-addin.c                                     \
	workbench/ide-workbench-header-bar.c                                \
	workbench/ide-workbench-message.c                                   \
	workbench/ide-workbench-open.c                                      \
	workbench/ide-workbench.c                                           \
	workers/ide-worker.c                                                \
	$(NULL)

libide_1_0_la_SOURCES =                                                     \
	$(libide_1_0_la_public_headers)                                     \
	$(libide_1_0_la_public_sources)                                     \
	application/ide-application-actions.c                               \
	application/ide-application-actions.h                               \
	application/ide-application-command-line.c                          \
	application/ide-application-plugins.c                               \
	application/ide-application-private.h                               \
	application/ide-application-tests.c                                 \
	application/ide-application-tests.h                                 \
	buildconfig/ide-buildconfig-plugin.c                                \
	buildconfig/ide-buildconfig-pipeline-addin.c                        \
	buildconfig/ide-buildconfig-pipeline-addin.h                        \
	buildsystem/ide-build-log.c                                         \
	buildsystem/ide-build-log-private.h                                 \
	buildsystem/ide-build-stage-private.h                               \
	buildui/ide-build-configuration-row.c                               \
	buildui/ide-build-configuration-row.h                               \
	buildui/ide-build-configuration-view.c                              \
	buildui/ide-build-configuration-view.h                              \
	buildui/ide-build-log-panel.c                                       \
	buildui/ide-build-log-panel.h                                       \
	buildui/ide-build-panel.c                                           \
	buildui/ide-build-panel.h                                           \
	buildui/ide-build-perspective.c                                     \
	buildui/ide-build-perspective.h                                     \
	buildui/ide-build-plugin.c                                          \
	buildui/ide-build-tool.c                                            \
	buildui/ide-build-tool.h                                            \
	buildui/ide-build-workbench-addin.c                                 \
	buildui/ide-build-workbench-addin.h                                 \
	buildui/ide-environment-editor-row.c                                \
	buildui/ide-environment-editor-row.h                                \
	buildui/ide-environment-editor.c                                    \
	buildui/ide-environment-editor.h                                    \
	debugger/ide-debugger-controls.c                                    \
	debugger/ide-debugger-controls.h                                    \
	debugger/ide-debugger-plugin.c                                      \
	editor/ide-editor-frame-actions.c                                   \
	editor/ide-editor-frame-actions.h                                   \
	editor/ide-editor-frame-private.h                                   \
	editor/ide-editor-frame.c                                           \
	editor/ide-editor-frame.h                                           \
	editor/ide-editor-map-bin.c                                         \
	editor/ide-editor-map-bin.h                                         \
	editor/ide-editor-plugin.c                                          \
	editor/ide-editor-layout-stack-addin.c                              \
	editor/ide-editor-layout-stack-addin.h                              \
	editor/ide-editor-layout-stack-controls.c                           \
	editor/ide-editor-layout-stack-controls.h                           \
	editor/ide-editor-print-operation.c                                 \
	editor/ide-editor-print-operation.h                                 \
	editor/ide-editor-spell-navigator.c                                 \
	editor/ide-editor-spell-navigator.h                                 \
	editor/ide-editor-spell-utils.c                                     \
	editor/ide-editor-spell-utils.h                                     \
	editor/ide-editor-spell-dict.c                                      \
	editor/ide-editor-spell-dict.h                                      \
	editor/ide-editor-spell-language-popover.c                          \
	editor/ide-editor-spell-language-popover.h                          \
	editor/ide-editor-spell-widget.c                                    \
	editor/ide-editor-spell-widget.h                                    \
	editor/ide-editor-tweak-widget.c                                    \
	editor/ide-editor-tweak-widget.h                                    \
	editor/ide-editor-view-actions.c                                    \
	editor/ide-editor-view-actions.h                                    \
	editor/ide-editor-view-addin-private.h                              \
	editor/ide-editor-view-private.h                                    \
	editor/ide-editor-workbench-addin.c                                 \
	editor/ide-editor-workbench-addin.h                                 \
	gconstructor.h                                                      \
	greeter/ide-greeter-perspective.c                                   \
	greeter/ide-greeter-perspective.h                                   \
	greeter/ide-greeter-project-row.c                                   \
	greeter/ide-greeter-project-row.h                                   \
	gsettings/ide-gsettings-file-settings.c                             \
	gsettings/ide-gsettings-file-settings.h                             \
	gsettings/ide-language-defaults.c                                   \
	gsettings/ide-language-defaults.h                                   \
	history/ide-back-forward-list-private.h                             \
	ide-internal.h                                                      \
	keybindings/ide-keybindings.c                                       \
	keybindings/ide-keybindings.h                                       \
	keybindings/ide-shortcuts-window.c                                  \
	keybindings/ide-shortcuts-window.h                                  \
	modelines/ide-modelines-file-settings.c                             \
	modelines/ide-modelines-file-settings.h                             \
	modelines/modeline-parser.c                                         \
	modelines/modeline-parser.h                                         \
	plugins/ide-extension-util.c                                        \
	plugins/ide-extension-util.h                                        \
	projects/ide-project-edit-private.h                                 \
	preferences/ide-preferences-bin-private.h                           \
	preferences/ide-preferences-bin.c                                   \
	preferences/ide-preferences-builtin.c                               \
	preferences/ide-preferences-builtin.h                               \
	preferences/ide-preferences-group-private.h                         \
	preferences/ide-preferences-language-row.c                          \
	preferences/ide-preferences-language-row.h                          \
	preferences/ide-preferences-page-private.h                          \
	preferences/ide-preferences-perspective.c                           \
	preferences/ide-preferences-perspective.h                           \
	runner/ide-run-manager-private.h                                    \
	search/ide-search-reducer.c                                         \
	snippets/ide-source-snippet-completion-item.c                       \
	snippets/ide-source-snippet-completion-item.h                       \
	snippets/ide-source-snippet-completion-provider.c                   \
	snippets/ide-source-snippet-completion-provider.h                   \
	snippets/ide-source-snippet-parser.c                                \
	snippets/ide-source-snippet-parser.h                                \
	snippets/ide-source-snippet-private.h                               \
	sourceview/ide-line-change-gutter-renderer.c                        \
	sourceview/ide-line-change-gutter-renderer.h                        \
	sourceview/ide-line-diagnostics-gutter-renderer.c                   \
	sourceview/ide-line-diagnostics-gutter-renderer.h                   \
	sourceview/ide-source-iter.c                                        \
	sourceview/ide-source-iter.h                                        \
	sourceview/ide-source-view-capture.c                                \
	sourceview/ide-source-view-capture.h                                \
	sourceview/ide-source-view-movements.c                              \
	sourceview/ide-source-view-movements.h                              \
	sourceview/ide-text-iter.c                                          \
	sourceview/ide-text-iter.h                                          \
	sourceview/ide-text-util.c                                          \
	sourceview/ide-text-util.h                                          \
	subprocess/ide-breakout-subprocess.c                                \
	subprocess/ide-breakout-subprocess.h                                \
	subprocess/ide-breakout-subprocess-private.h                        \
	subprocess/ide-simple-subprocess.c                                  \
	subprocess/ide-simple-subprocess.h                                  \
	theatrics/ide-box-theatric.c                                        \
	theatrics/ide-box-theatric.h                                        \
	theming/ide-css-provider.c                                          \
	theming/ide-css-provider.h                                          \
	theming/ide-theme-manager.c                                         \
	theming/ide-theme-manager.h                                         \
	tree/ide-tree-private.h                                             \
	util/ide-async-helper.c                                             \
	util/ide-async-helper.h                                             \
	util/ide-battery-monitor.c                                          \
	util/ide-battery-monitor.h                                          \
	util/ide-doc-seq.c                                                  \
	util/ide-doc-seq.h                                                  \
	util/ide-gdk.c                                                      \
	util/ide-gdk.h                                                      \
	util/ide-ref-ptr.c                                                  \
	util/ide-ref-ptr.h                                                  \
	util/ide-window-settings.c                                          \
	util/ide-window-settings.h                                          \
	workbench/ide-layout-stack-actions.c                                \
	workbench/ide-layout-stack-actions.h                                \
	workbench/ide-layout-stack-private.h                                \
	workbench/ide-layout-tab-bar.c                                      \
	workbench/ide-layout-tab-bar.h                                      \
	workbench/ide-layout-tab-bar-private.h                              \
	workbench/ide-layout-tab.c                                          \
	workbench/ide-layout-tab.h                                          \
	workbench/ide-layout-tab-private.h                                  \
	workbench/ide-omni-bar-row.c                                        \
	workbench/ide-omni-bar-row.h                                        \
	workbench/ide-perspective-menu-button.c                             \
	workbench/ide-perspective-menu-button.h                             \
	workbench/ide-workbench-actions.c                                   \
	workbench/ide-workbench-private.h                                   \
	workers/ide-worker-manager.c                                        \
	workers/ide-worker-manager.h                                        \
	workers/ide-worker-process.c                                        \
	workers/ide-worker-process.h                                        \
	$(NULL)

libide_1_0_la_includes =                                                    \
	$(DEBUG_CFLAGS)                                                     \
	-DPACKAGE_DATADIR="\"${datadir}\""                                  \
	-DPACKAGE_DOCDIR="\"${docdir}\""                                    \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\"                        \
	-DPACKAGE_LIBDIR=\""${libdir}"\"                                    \
	-DPACKAGE_LIBEXECDIR=\""${libexecdir}"\"                            \
	-DBUILDDIR=\""${abs_top_builddir}"\"                                \
	-DSRCDIR=\""${abs_top_srcdir}"\"                                    \
	-I$(srcdir)                                                         \
	-I$(top_builddir)/contrib/jsonrpc-glib                              \
	-I$(top_builddir)/contrib/pnl                                       \
	-I$(top_builddir)/contrib/tmpl                                      \
	-I$(top_builddir)/data/icons/hicolor                                \
	-I$(top_builddir)/libide                                            \
	-I$(top_srcdir)/contrib/egg                                         \
	-I$(top_srcdir)/contrib/gd                                          \
	-I$(top_srcdir)/contrib/jsonrpc-glib                                \
	-I$(top_srcdir)/contrib/nautilus                                    \
	-I$(top_srcdir)/contrib/pnl                                         \
	-I$(top_srcdir)/contrib/search                                      \
	-I$(top_srcdir)/contrib/tmpl                                        \
	-I$(top_srcdir)/contrib/xml                                         \
	$(NULL)

libide_1_0_la_CPPFLAGS =                                                    \
	$(AM_CPPFLAGS)                                                      \
	$(libide_1_0_la_includes)                                           \
	$(NULL)

libide_1_0_la_CFLAGS =                                                      \
	$(AM_CFLAGS)                                                        \
	$(LIBIDE_CFLAGS)                                                    \
	$(OPTIMIZE_CFLAGS)                                                  \
	$(DEBUG_CFLAGS)                                                     \
	$(libide_1_0_la_includes)                                           \
	$(NULL)

libide_1_0_la_CXXFLAGS =                                                    \
	$(AM_CXXFLAGS)                                                      \
	$(LIBIDE_CFLAGS)                                                    \
	$(libide_1_0_la_includes)                                           \
	$(OPTIMIZE_CFLAGS)                                                  \
	$(NULL)

libide_1_0_la_LDFLAGS =                                                     \
	$(OPTIMIZE_LDFLAGS)                                                 \
	-avoid-version                                                      \
	-no-undefined                                                       \
	--no-as-needed                                                      \
	-export-symbols-regex '^(ide_|_ide_).*'                             \
	$(NULL)

libide_1_0_la_LIBADD =                                                      \
	$(LIBIDE_LIBS)                                                      \
	$(SHM_LIB)                                                          \
	-lm                                                                 \
	$(top_builddir)/data/icons/hicolor/libicons.la                      \
	$(top_builddir)/contrib/egg/libegg-private.la                       \
	$(top_builddir)/contrib/gd/libgd-private.la                         \
	$(top_builddir)/contrib/jsonrpc-glib/libjsonrpc-glib.la             \
	$(top_builddir)/contrib/nautilus/libnautilus.la                     \
	$(top_builddir)/contrib/pnl/libpanel-gtk.la                         \
	$(top_builddir)/contrib/search/libsearch.la                         \
	$(top_builddir)/contrib/tmpl/libtemplate-glib-1.0.la                \
	$(top_builddir)/contrib/xml/libxml-private.la                       \
	$(NULL)

if ENABLE_WEBKIT
libide_1_0_la_SOURCES += webkit/ide-webkit.c
libide_1_0_la_CFLAGS += $(WEBKIT_CFLAGS)
libide_1_0_la_LIBADD += $(WEBKIT_LIBS)
endif


libide_1_0_la_built_sources =                                               \
	resources/ide-resources.c                                           \
	resources/ide-resources.h                                           \
	$(NULL)


if ENABLE_EDITORCONFIG
libide_1_0_la_SOURCES +=                                                    \
	editorconfig/editorconfig-glib.c                                    \
	editorconfig/editorconfig-glib.h                                    \
	editorconfig/ide-editorconfig-file-settings.c                       \
	editorconfig/ide-editorconfig-file-settings.h                       \
        $(NULL)

libide_1_0_la_includes +=                                                   \
	-I$(top_srcdir)/contrib/libeditorconfig                             \
	-I$(srcdir)/editorconfig                                            \
        $(NULL)

libide_1_0_la_CFLAGS += -DENABLE_EDITORCONFIG
libide_1_0_la_LIBADD += $(top_builddir)/contrib/libeditorconfig/libeditorconfig.la
endif


glib_enum_headers =                                                         \
	buffers/ide-buffer.h                                                \
	buildsystem/ide-build-log.h                                         \
	buildsystem/ide-build-pipeline.h                                    \
	devices/ide-device.h                                                \
	diagnostics/ide-diagnostic.h                                        \
	doap/ide-doap.h                                                     \
	files/ide-indent-style.h                                            \
	highlighting/ide-highlighter.h                                      \
	runtimes/ide-runtime.h                                              \
	sourceview/ide-source-view.h                                        \
	symbols/ide-symbol.h                                                \
	threading/ide-thread-pool.h                                         \
	vcs/ide-vcs-config.h                                                \
	workbench/ide-layout-stack-split.h                                  \
	$(NULL)

glib_enum_h = ide-enums.h
glib_enum_c = ide-enums.c

include $(top_srcdir)/build/autotools/Makefile.am.enums


glib_resources_c = resources/ide-resources.c
glib_resources_h = resources/ide-resources.h
glib_resources_xml = resources/libide.gresource.xml
glib_resources_namespace = ide
include $(top_srcdir)/build/autotools/Makefile.am.gresources

nodist_libide_1_0_la_SOURCES =                                              \
	ide-enums.h                                                         \
	ide-enums.c                                                         \
	resources/ide-resources.c                                           \
	resources/ide-resources.h                                           \
	$(NULL)


if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS =                                               \
	--includedir=$(srcdir)                                              \
	--includedir=$(top_builddir)/contrib/egg                            \
	--includedir=$(top_builddir)/contrib/pnl                            \
	--includedir=$(top_builddir)/contrib/tmpl                           \
        $(NULL)

introspection_sources =                                                     \
	$(libide_1_0_la_public_sources)                                     \
	$(libide_1_0_la_public_headers)                                     \
	ide-enums.h                                                         \
	ide-enums.c                                                         \
	$(NULL)

Ide-1.0.gir: libide-1.0.la
Ide_1_0_gir_INCLUDES = Gio-2.0 GtkSource-3.0 Peas-1.0 Json-1.0
Ide_1_0_gir_CFLAGS = $(libide_1_0_la_CFLAGS)
Ide_1_0_gir_LIBS =                                                          \
	libide-1.0.la                                                       \
	$(top_builddir)/contrib/egg/libegg-private.la                       \
	$(top_builddir)/contrib/pnl/libpanel-gtk.la                         \
	$(top_builddir)/contrib/tmpl/libtemplate-glib-1.0.la                \
        $(NULL)
Ide_1_0_gir_FILES = $(introspection_sources)
Ide_1_0_gir_SCANNERFLAGS =                                                  \
	--include-uninstalled=$(top_builddir)/contrib/egg/Egg-1.0.gir       \
	--include-uninstalled=$(top_builddir)/contrib/pnl/Pnl-1.0.gir       \
	--include-uninstalled=$(top_builddir)/contrib/tmpl/Template-1.0.gir \
	--c-include="ide.h"                                                 \
	-n Ide                                                              \
	$(NULL)
INTROSPECTION_GIRS += Ide-1.0.gir

girdir = $(datadir)/gnome-builder/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(pkglibdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif


if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

libide-1.0.vapi: Ide-1.0.gir

VAPIGEN_VAPIS = libide-1.0.vapi

libide_1_0_vapi_DEPS =                                                      \
	gio-2.0                                                             \
	gtk+-3.0                                                            \
	gtksourceview-3.0                                                   \
	libpeas-1.0                                                         \
	panel-gtk                                                           \
	template-glib-1.0                                                   \
	egg-private                                                         \
	json-glib-1.0                                                       \
        $(NULL)

libide_1_0_vapi_METADATADIRS = $(srcdir)
libide_1_0_vapi_FILES = Ide-1.0.gir
libide_1_0_vapi_VAPIDIRS =                                                  \
	$(top_builddir)/contrib/egg                                         \
	$(top_builddir)/contrib/pnl                                         \
	$(top_builddir)/contrib/tmpl                                        \
        $(NULL)
libide_1_0_vapi_GIRDIRS =                                                   \
	$(top_builddir)/contrib/egg                                         \
	$(top_builddir)/contrib/pnl                                         \
	$(top_builddir)/contrib/tmpl                                        \
        $(NULL)

libide-1.0.deps: Makefile
	$(AM_V_GEN) echo $(libide_1_0_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/gnome-builder/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

EXTRA_DIST += libide-1.0.deps

DISTCLEANFILES += $(vapi_DATA)
endif


if ENABLE_PYTHON_SCRIPTING
overridesdir = $(pyoverridesdir)
overrides_PYTHON = Ide.py
endif


-include $(top_srcdir)/git.mk
