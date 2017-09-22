if get_option('with_todo')

todo_resources = gnome.compile_resources(
  'todo-resources',
  'todo.gresource.xml',
  c_name: 'gbp_todo',
)

todo_sources = [
  'gbp-todo-item.c',
  'gbp-todo-item.h',
  'gbp-todo-model.c',
  'gbp-todo-model.h',
  'gbp-todo-plugin.c',
  'gbp-todo-panel.c',
  'gbp-todo-panel.h',
  'gbp-todo-workbench-addin.c',
  'gbp-todo-workbench-addin.h',
]

gnome_builder_plugins_sources += files(todo_sources)
gnome_builder_plugins_sources += todo_resources[0]

endif
