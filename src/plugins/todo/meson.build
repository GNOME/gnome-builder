if get_option('plugin_todo')

plugins_sources += files([
  'gbp-todo-item.c',
  'gbp-todo-model.c',
  'gbp-todo-panel.c',
  'gbp-todo-workspace-addin.c',
  'todo-plugin.c',
])

plugin_todo_resources = gnome.compile_resources(
  'todo-resources',
  'todo.gresource.xml',
  c_name: 'gbp_todo',
)

plugins_sources += plugin_todo_resources

endif
