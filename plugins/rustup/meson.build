if get_option('with_rustup')

rustup_dir = join_paths(plugindir, 'rustup_plugin')
install_data('rustup.plugin', install_dir: plugindir)
install_data('rustup_plugin/__init__.py', install_dir: rustup_dir)
install_subdir('rustup_plugin/resources', install_dir: rustup_dir)


endif
