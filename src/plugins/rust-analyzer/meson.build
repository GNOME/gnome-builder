if get_option('plugin_rust_analyzer')

install_data('rust_analyzer_plugin.py', install_dir: plugindir)

configure_file(
          input: 'rust-analyzer.plugin',
         output: 'rust-analyzer.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
