if get_option('plugin_go_langserv')

install_data('go_langserver_plugin.py', install_dir: plugindir)

configure_file(
          input: 'go-langserv.plugin',
         output: 'go-langserv.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
