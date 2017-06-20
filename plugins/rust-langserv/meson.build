if get_option('with_rust_langserv')

install_data('rust_langserv_plugin.py', install_dir: plugindir)

configure_file(
          input: 'rust-langserv.plugin',
         output: 'rust-langserv.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
