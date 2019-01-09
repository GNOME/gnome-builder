if get_option('plugin_npm')

install_data('npm_plugin.py', install_dir: plugindir)

configure_file(
          input: 'npm.plugin',
         output: 'npm.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
