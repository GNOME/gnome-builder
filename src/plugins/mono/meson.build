if get_option('plugin_mono')

install_data('mono_plugin.py', install_dir: plugindir)

configure_file(
          input: 'mono.plugin',
         output: 'mono.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
