if get_option('with_vala_langserv')

install_data('vala_langserv_plugin.py', install_dir: plugindir)

configure_file(
          input: 'vala-langserv.plugin',
         output: 'vala-langserv.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
