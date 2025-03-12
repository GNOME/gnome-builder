libide_gui_header_dir = join_paths(libide_header_dir, 'gui')
libide_gui_header_subdir = join_paths(libide_header_subdir, 'gui')
libide_include_directories += include_directories('.')

libide_gui_generated_headers = []

#
# Public API Headers
#

libide_gui_public_headers = [
  'ide-application.h',
  'ide-application-addin.h',
  'ide-application-tweaks.h',
  'ide-environment-editor.h',
  'ide-frame.h',
  'ide-frame-addin.h',
  'ide-grid.h',
  'ide-gui-global.h',
  'ide-header-bar.h',
  'ide-marked-view.h',
  'ide-notifications-button.h',
  'ide-omni-bar-addin.h',
  'ide-omni-bar.h',
  'ide-page.h',
  'ide-pane.h',
  'ide-panel-position.h',
  'ide-primary-workspace.h',
  'ide-run-button.h',
  'ide-search-popover.h',
  'ide-session.h',
  'ide-session-item.h',
  'ide-shortcut-info.h',
  'ide-shortcut-provider.h',
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
  'ide-application-private.h',
  'ide-environment-editor-row.h',
  'ide-notification-list-box-row-private.h',
  'ide-notification-stack-private.h',
  'ide-notification-view-private.h',
  'ide-page-private.h',
  'ide-recoloring-private.h',
  'ide-search-popover-private.h',
  'ide-shortcut-bundle-private.h',
  'ide-shortcut-manager-private.h',
  'ide-shortcut-observer-private.h',
  'ide-shortcut-window-private.h',
  'ide-style-variant-preview-private.h',
  'ide-support-private.h',
]

libide_gui_private_sources = [
  'ide-application-actions.c',
  'ide-application-color.c',
  'ide-application-plugins.c',
  'ide-application-settings.c',
  'ide-application-tweaks.c',
  'ide-environment-editor-row.c',
  'ide-notification-list-box-row.c',
  'ide-notification-stack.c',
  'ide-notification-view.c',
  'ide-recoloring.c',
  'ide-search-popover.c',
  'ide-shortcut-bundle.c',
  'ide-shortcut-manager.c',
  'ide-shortcut-observer.c',
  'ide-support.c',
  'ide-style-variant-preview.c',
  'ide-workbench-session.c',
  'ide-workspace-session.c',
]

libide_gui_public_sources = [
  'ide-application.c',
  'ide-application-addin.c',
  'ide-application-command-line.c',
  'ide-application-open.c',
  'ide-environment-editor.c',
  'ide-frame-addin.c',
  'ide-frame.c',
  'ide-grid.c',
  'ide-gui-global.c',
  'ide-header-bar.c',
  'ide-marked-view.c',
  'ide-notifications-button.c',
  'ide-omni-bar-addin.c',
  'ide-omni-bar.c',
  'ide-page.c',
  'ide-pane.c',
  'ide-panel-position.c',
  'ide-primary-workspace.c',
  'ide-run-button.c',
  'ide-session.c',
  'ide-session-item.c',
  'ide-shortcut-provider.c',
  'ide-shortcut-window.c',
  'ide-workbench.c',
  'ide-workbench-addin.c',
  'ide-workspace.c',
  'ide-workspace-addin.c',
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
  cc.find_library('m', required: false),

  libgio_dep,
  libgir_dep,
  libgtk_dep,
  libgtksource_dep,
  libadwaita_dep,
  libpanel_dep,
  libpeas_dep,
  libjson_glib_dep,

  dependency('libcmark', version: '>= 0.29.0'),

  libide_core_dep,
  libide_io_dep,
  libide_foundry_dep,
  libide_gtk_dep,
  libide_debugger_dep,
  libide_plugins_dep,
  libide_projects_dep,
  libide_search_dep,
  libide_sourceview_dep,
  libide_tweaks_dep,
]

if get_option('webkit').enabled()
  libide_gui_deps += [libwebkit_dep]
endif

#
# Library Definitions
#

libide_gui = static_library('ide-gui-' + libide_api_version, libide_gui_sources,
         dependencies: libide_gui_deps,
               c_args: libide_args + release_args + ['-DIDE_GUI_COMPILATION'],
  include_directories: [include_directories('..')]
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
gnome_builder_gir_extra_args += '-DIDE_GUI_COMPILATION'
gnome_builder_main_headers += 'libide-gui.h'
