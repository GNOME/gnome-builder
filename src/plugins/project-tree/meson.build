if get_option('with_project_tree')

project_tree_resources = gnome.compile_resources(
  'project-tree-resources',
  'project-tree.gresource.xml',
  c_name: 'gb_project_tree',
)

project_tree_sources = [
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

gnome_builder_plugins_deps += dependency('vte-2.91', version: '>=0.40.2')
gnome_builder_plugins_args += '-DHAVE_VTE'

gnome_builder_plugins_sources += files(project_tree_sources)
gnome_builder_plugins_sources += project_tree_resources[0]

endif
