if get_option('plugin_jedi_language_server')

install_data('jedi_language_server_plugin.py', install_dir: plugindir)

configure_file(
          input: 'jedi-language-server.plugin',
         output: 'jedi-language-server.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
