if get_option('with_d_langserv')

install_data('d_langserver_plugin.py', install_dir: plugindir)

configure_file(
          input: 'd-langserv.plugin',
         output: 'd-langserv.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
