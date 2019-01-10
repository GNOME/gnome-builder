if get_option('plugin_jedi')

install_data('jedi_plugin.py', install_dir: plugindir)

configure_file(
          input: 'jedi.plugin',
         output: 'jedi.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
