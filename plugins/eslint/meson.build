if get_option('with_eslint')

eslint_dir = join_paths(plugindir, 'eslint_plugin')
install_data('eslint.plugin', install_dir: plugindir)
install_data('eslint_plugin/__init__.py', install_dir: eslint_dir)

install_data('org.gnome.builder.plugins.eslint.gschema.xml',
  install_dir: schema_dir)

endif
