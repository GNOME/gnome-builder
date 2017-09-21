if get_option('with_cmake')

install_data('cmake_plugin/__init__.py', install_dir: join_paths(plugindir, 'cmake_plugin'))

configure_file(
          input: 'cmake.plugin',
         output: 'cmake.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
