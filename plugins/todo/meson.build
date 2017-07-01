if get_option('with_todo')

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

shared_module('todo-plugin', todo_sources,
  dependencies: plugin_deps,
     link_args: plugin_link_args,
  link_depends: plugin_link_deps,
       install: true,
   install_dir: plugindir,
)

configure_file(
          input: 'todo.plugin',
         output: 'todo.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
