libide_gui_header_subdir = join_paths(libide_header_subdir, 'gui')
libide_include_directories += include_directories('.')

libide_gui_generated_headers = []

#
# Public API Headers
#

libide_gui_public_headers = [
  'ide-application.h',
  'ide-application-addin.h',
  'ide-cell-renderer-fancy.h',
  'ide-command.h',
  'ide-command-manager.h',
  'ide-command-provider.h',
  'ide-config-view-addin.h',
  'ide-environment-editor.h',
  'ide-fancy-tree-view.h',
  'ide-frame-addin.h',
  'ide-frame-empty-state.h',
  'ide-frame-header.h',
  'ide-frame.h',
  'ide-grid-column.h',
  'ide-grid.h',
  'ide-gui-global.h',
  'ide-header-bar.h',
  'ide-marked-view.h',
  'ide-notifications-button.h',
  'ide-omni-bar-addin.h',
  'ide-omni-bar.h',
  'ide-page.h',
  'ide-pane.h',
  'ide-panel.h',
  'ide-preferences-addin.h',
  'ide-preferences-surface.h',
  'ide-preferences-window.h',
  'ide-primary-workspace.h',
  'ide-search-button.h',
  'ide-session-addin.h',
  'ide-surface.h',
  'ide-surfaces-button.h',
  'ide-tagged-entry.h',
  'ide-transfer-button.h',
  'ide-transient-sidebar.h',
  'ide-worker.h',
  'ide-workbench.h',
  'ide-workbench-addin.h',
  'ide-workspace.h',
  'ide-workspace-addin.h',
  'libide-gui.h',
]

install_headers(libide_gui_public_headers, subdir: libide_gui_header_subdir)

#
# Sources
#

libide_gui_private_headers = [
  'gs-markdown-private.h',
  'ide-application-private.h',
  'ide-environment-editor-row.h',
  'ide-frame-wrapper.h',
  'ide-gui-private.h',
  'ide-keybindings.h',
  'ide-notification-list-box-row-private.h',
  'ide-notifications-button-popover-private.h',
  'ide-notification-stack-private.h',
  'ide-notification-view-private.h',
  'ide-preferences-builtin-private.h',
  'ide-preferences-language-row-private.h',
  'ide-run-button.h',
  'ide-session-private.h',
  'ide-window-settings-private.h',
  'ide-shortcut-label-private.h',
  'ide-shortcuts-window-private.h',
  'ide-worker-manager.h',
  'ide-worker-process.h',
]

libide_gui_private_sources = [
  'gs-markdown.c',
  'ide-application-actions.c',
  'ide-application-color.c',
  'ide-application-shortcuts.c',
  'ide-application-plugins.c',
  'ide-environment-editor-row.c',
  'ide-frame-actions.c',
  'ide-frame-shortcuts.c',
  'ide-frame-wrapper.c',
  'ide-grid-actions.c',
  'ide-grid-column-actions.c',
  'ide-header-bar-shortcuts.c',
  'ide-keybindings.c',
  'ide-notification-list-box-row.c',
  'ide-notification-stack.c',
  'ide-notification-view.c',
  'ide-notifications-button-popover.c',
  'ide-preferences-builtin.c',
  'ide-preferences-language-row.c',
  'ide-primary-workspace-actions.c',
  'ide-run-button.c',
  'ide-session.c',
  'ide-shortcuts-window.c',
  'ide-window-settings.c',
  'ide-worker-manager.c',
  'ide-worker-process.c',
  'ide-workspace-actions.c',
]

libide_gui_public_sources = [
  'ide-application.c',
  'ide-application-addin.c',
  'ide-application-command-line.c',
  'ide-application-open.c',
  'ide-cell-renderer-fancy.c',
  'ide-command.c',
  'ide-command-manager.c',
  'ide-command-provider.c',
  'ide-config-view-addin.c',
  'ide-environment-editor.c',
  'ide-fancy-tree-view.c',
  'ide-frame-addin.c',
  'ide-frame-empty-state.c',
  'ide-frame-header.c',
  'ide-frame.c',
  'ide-grid-column.c',
  'ide-grid.c',
  'ide-gui-global.c',
  'ide-header-bar.c',
  'ide-marked-view.c',
  'ide-notifications-button.c',
  'ide-omni-bar-addin.c',
  'ide-omni-bar.c',
  'ide-page.c',
  'ide-pane.c',
  'ide-panel.c',
  'ide-primary-workspace.c',
  'ide-preferences-addin.c',
  'ide-preferences-surface.c',
  'ide-preferences-window.c',
  'ide-search-button.c',
  'ide-session-addin.c',
  'ide-shortcut-label.c',
  'ide-surface.c',
  'ide-surfaces-button.c',
  'ide-tagged-entry.c',
  'ide-transient-sidebar.c',
  'ide-transfer-button.c',
  'ide-workbench.c',
  'ide-workbench-addin.c',
  'ide-workspace.c',
  'ide-workspace-addin.c',
  'ide-worker.c',
]

libide_gui_sources = libide_gui_public_sources + libide_gui_private_sources

#
# Generated Resource Files
#

libide_gui_resources = gnome.compile_resources(
  'ide-gui-resources',
  'libide-gui.gresource.xml',
  c_name: 'ide_gui',
)
libide_gui_generated_headers += [libide_gui_resources[1]]
libide_gui_sources += libide_gui_resources


#
# Dependencies
#

libide_gui_deps = [
  libgio_dep,
  libgtk_dep,
  libgtksource_dep,
  libdazzle_dep,
  libpeas_dep,
  libwebkit_dep,

  libide_core_dep,
  libide_io_dep,
  libide_foundry_dep,
  libide_debugger_dep,
  libide_plugins_dep,
  libide_projects_dep,
  libide_search_dep,
  libide_themes_dep,
]

#
# Library Definitions
#

libide_gui = static_library('ide-gui-' + libide_api_version, libide_gui_sources,
   dependencies: libide_gui_deps,
         c_args: libide_args + release_args + ['-DIDE_GUI_COMPILATION'],
)

libide_gui_dep = declare_dependency(
              sources: libide_gui_private_headers + libide_gui_generated_headers,
         dependencies: libide_gui_deps,
            link_with: libide_gui,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_gui_public_sources)
gnome_builder_public_headers += files(libide_gui_public_headers)
gnome_builder_private_sources += files(libide_gui_private_sources)
gnome_builder_private_headers += files(libide_gui_private_headers)
gnome_builder_include_subdirs += libide_gui_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-gui.h', '-DIDE_GUI_COMPILATION']
