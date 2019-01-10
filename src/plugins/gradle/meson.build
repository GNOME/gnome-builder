if get_option('plugin_gradle')

install_data('gradle_plugin.py', install_dir: plugindir)

configure_file(
          input: 'gradle.plugin',
         output: 'gradle.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
