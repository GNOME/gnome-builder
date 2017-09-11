if get_option('with_gjs_symbols')

install_data('gjs_symbols.py', install_dir: plugindir)

configure_file(
          input: 'gjs_symbols.plugin',
         output: 'gjs_symbols.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
