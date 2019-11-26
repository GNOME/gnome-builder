if get_option('plugin_stylelint')

install_data('stylelint_plugin.py', install_dir: plugindir)

install_data('org.gnome.builder.plugins.stylelint.gschema.xml',
  install_dir: schema_dir)

configure_file(
          input: 'stylelint.plugin',
         output: 'stylelint.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
