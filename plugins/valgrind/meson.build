if get_option('with_valgrind')

install_data('valgrind_plugin/__init__.py', install_dir: join_paths(plugindir, 'valgrind_plugin'))

configure_file(
          input: 'valgrind.plugin',
         output: 'valgrind.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
