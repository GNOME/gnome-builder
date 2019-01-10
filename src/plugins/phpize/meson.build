if get_option('plugin_phpize')

install_data('phpize_plugin.py', install_dir: plugindir)

configure_file(
          input: 'phpize.plugin',
         output: 'phpize.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
