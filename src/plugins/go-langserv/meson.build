if get_option('with_go_langserv')

install_data('go_langserver_plugin.py', install_dir: plugindir)

configure_file(
          input: 'go-langserv.plugin',
         output: 'go-langserv.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
