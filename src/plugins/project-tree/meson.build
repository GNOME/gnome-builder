plugins_sources += files([
  'project-tree-plugin.c',
  'gbp-project-tree.c',
  'gbp-project-tree-addin.c',
  'gbp-project-tree-frame-addin.c',
  'gbp-project-tree-pane.c',
  'gbp-project-tree-pane-actions.c',
  'gbp-project-tree-tweaks-addin.c',
  'gbp-project-tree-workspace-addin.c',
  'gbp-new-file-popover.c',
  'gbp-rename-file-popover.c',
])

plugin_project_tree_resources = gnome.compile_resources(
  'gbp-project-tree-resources',
  'project-tree.gresource.xml',
  c_name: 'gbp_project_tree',
)

plugins_sources += plugin_project_tree_resources
