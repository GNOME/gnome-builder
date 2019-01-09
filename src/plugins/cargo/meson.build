if get_option('plugin_cargo')

install_data('cargo_plugin.py', install_dir: plugindir)

configure_file(
          input: 'cargo.plugin',
         output: 'cargo.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
