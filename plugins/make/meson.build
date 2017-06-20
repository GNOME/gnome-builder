if get_option('with_make')

install_data('make_plugin/__init__.py', install_dir: join_paths(plugindir, 'make_plugin'))

configure_file(
          input: 'make.plugin',
         output: 'make.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
