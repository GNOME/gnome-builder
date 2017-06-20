if get_option('with_todo')

install_subdir('todo_plugin', install_dir: plugindir)

configure_file(
          input: 'todo.plugin',
         output: 'todo.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
