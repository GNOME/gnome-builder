if get_option('with_eslint')

eslint_dir = join_paths(plugindir, 'eslint_plugin')
install_data('eslint_plugin/__init__.py', install_dir: eslint_dir)

install_data('org.gnome.builder.plugins.eslint.gschema.xml',
  install_dir: schema_dir)

configure_file(
          input: 'eslint.plugin',
         output: 'eslint.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
