if get_option('plugin_rubocop')

install_data('rubocop_plugin.py', install_dir: plugindir)

install_data('org.gnome.builder.plugins.rubocop.gschema.xml',
  install_dir: schema_dir)

configure_file(
          input: 'rubocop.plugin',
         output: 'rubocop.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
