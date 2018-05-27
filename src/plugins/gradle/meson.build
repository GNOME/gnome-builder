if get_option('with_gradle')

install_data('gradle_plugin.py', install_dir: plugindir)

configure_file(
          input: 'gradle.plugin',
         output: 'gradle.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
