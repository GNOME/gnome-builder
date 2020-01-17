if get_option('plugin_copyright')

install_data('copyright_plugin.py', install_dir: plugindir)
install_data('org.gnome.builder.plugins.copyright.gschema.xml', install_dir: schema_dir)

configure_file(
          input: 'copyright.plugin',
         output: 'copyright.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
