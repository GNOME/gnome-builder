if get_option('with_rustup')

rustup_dir = join_paths(plugindir, 'rustup_plugin')
install_data('rustup_plugin/__init__.py', install_dir: rustup_dir)
install_subdir('rustup_plugin/resources', install_dir: rustup_dir)

configure_file(
          input: 'rustup.plugin',
         output: 'rustup.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
