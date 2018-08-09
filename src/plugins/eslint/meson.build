if get_option('with_eslint')

install_data('eslint_plugin.py', install_dir: plugindir)

install_data('org.gnome.builder.plugins.eslint.gschema.xml',
  install_dir: schema_dir)

configure_file(
          input: 'eslint.plugin',
         output: 'eslint.plugin',
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif
