if get_option('with_clang_langserv')

find_program('clangd')

install_data('clang_langserv_plugin.py', install_dir: plugindir)

configure_file(
          input: 'clang-langserv.plugin',
         output: 'clang-langserv.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
