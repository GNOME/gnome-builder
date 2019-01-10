if get_option('plugin_maven')

install_data('maven_plugin.py', install_dir: plugindir)

configure_file(
          input: 'maven.plugin',
         output: 'maven.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
