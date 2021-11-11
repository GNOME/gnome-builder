if get_option('plugin_ts_language_server')

install_data('ts_language_server_plugin.py', install_dir: plugindir)

configure_file(
          input: 'ts-language-server.plugin',
         output: 'ts-language-server.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
