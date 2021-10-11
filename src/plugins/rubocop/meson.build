if get_option('plugin_rubocop')

install_data('rubocop_plugin.py', install_dir: plugindir)

configure_file(
          input: 'rubocop.plugin',
         output: 'rubocop.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
