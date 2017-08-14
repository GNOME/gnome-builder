if get_option('with_project_tree')

project_tree_resources = gnome.compile_resources(
  'gb-project-tree-resources',
  'gb-project-tree.gresource.xml',
  c_name: 'gb_project_tree',
)

project_tree_sources = [
  project_tree_resources[0],
  project_tree_resources[1],
  'gb-new-file-popover.c',
  'gb-new-file-popover.h',
  'gb-project-file.c',
  'gb-project-file.h',
  'gb-project-tree-actions.c',
  'gb-project-tree-actions.h',
  'gb-project-tree-builder.c',
  'gb-project-tree-builder.h',
  'gb-project-tree.c',
  'gb-project-tree.h',
  'gb-project-tree-editor-addin.c',
  'gb-project-tree-editor-addin.h',
  'gb-project-tree-private.h',
  'gb-project-tree-shortcuts.c',
  'gb-rename-file-popover.c',
  'gb-rename-file-popover.h',
  'gb-project-tree-addin.c',
  'gb-project-tree-addin.h',
  'project-tree-plugin.c',
]

project_tree_deps = plugin_deps
project_tree_cflags = []

if get_option('with_terminal')

project_tree_deps += dependency('vte-2.91', version: '>=0.40.2')
project_tree_cflags += '-DHAVE_VTE'

endif

shared_module('project-tree-plugin', project_tree_sources,
  dependencies: project_tree_deps,
  c_args: project_tree_cflags,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'project-tree.plugin',
         output: 'project-tree.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
