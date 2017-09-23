if get_option('with_make')

install_data('make_plugin.py', install_dir: plugindir)

configure_file(
          input: 'make.plugin',
         output: 'make.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
