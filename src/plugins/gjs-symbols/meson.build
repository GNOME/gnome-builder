if get_option('plugin_gjs_symbols')

install_data('gjs_symbols.py', install_dir: plugindir)

configure_file(
          input: 'gjs_symbols.plugin',
         output: 'gjs_symbols.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
