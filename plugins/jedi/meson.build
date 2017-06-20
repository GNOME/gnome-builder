if get_option('with_jedi')

install_data('jedi_plugin.py', install_dir: plugindir)

configure_file(
          input: 'jedi.plugin',
         output: 'jedi.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
