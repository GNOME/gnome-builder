libide_gui_header_subdir = join_paths(libide_header_subdir, 'gui')
libide_include_directories += include_directories('.')

libide_gui_generated_headers = []

#
# Public API Headers
#

libide_gui_public_headers = [
  'ide-application.h',
  'ide-application-addin.h',
  'ide-config-view-addin.h',
  'ide-environment-editor.h',
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
  'ide-preferences-addin.h',
  'ide-preferences-window.h',
  'ide-primary-workspace.h',
  'ide-run-button.h',
  'ide-session-addin.h',
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
  'ide-keybindings.h',
  'ide-notification-list-box-row-private.h',
  'ide-notification-stack-private.h',
  'ide-notification-view-private.h',
  'ide-preferences-builtin-private.h',
  'ide-primary-workspace-private.h',
  'ide-session-private.h',
]

libide_gui_private_sources = [
  'ide-application-actions.c',
  'ide-application-color.c',
  'ide-application-plugins.c',
  'ide-environment-editor-row.c',
  'ide-keybindings.c',
  'ide-notification-list-box-row.c',
  'ide-notification-stack.c',
  'ide-notification-view.c',
  'ide-preferences-builtin.c',
  'ide-primary-workspace-actions.c',
  'ide-session.c',
  'ide-workspace-actions.c',
]

libide_gui_public_sources = [
  'ide-application.c',
  'ide-application-addin.c',
  'ide-application-command-line.c',
  'ide-application-open.c',
  'ide-config-view-addin.c',
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
  'ide-primary-workspace.c',
  'ide-preferences-addin.c',
  'ide-preferences-window.c',
  'ide-run-button.c',
  'ide-session-addin.c',
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
  libgio_dep,
  libgtk_dep,
  libgtksource_dep,
  libadwaita_dep,
  libpanel_dep,
  libpeas_dep,

  dependency('libcmark', version: '>= 0.29.0'),

  libide_core_dep,
  libide_io_dep,
  libide_commands_dep,
  libide_foundry_dep,
  libide_gtk_dep,
  libide_debugger_dep,
  libide_plugins_dep,
  libide_projects_dep,
  libide_search_dep,
  libide_themes_dep,
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

subdir('tests')
