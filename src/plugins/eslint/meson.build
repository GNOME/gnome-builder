if get_option('plugin_eslint')

install_data('eslint_plugin.py', install_dir: plugindir)

configure_file(
          input: 'eslint.plugin',
         output: 'eslint.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
