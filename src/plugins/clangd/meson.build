if get_option('plugin_clangd')

install_data('clangd_plugin.py', install_dir: plugindir)

configure_file(
          input: 'clangd.plugin',
         output: 'clangd.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
