if get_option('with_find_other_file')

install_data('find_other_file.py', install_dir: plugindir)

configure_file(
          input: 'find-other-file.plugin',
         output: 'find-other-file.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
