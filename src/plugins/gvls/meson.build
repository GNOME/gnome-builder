if get_option('plugin_gvls')

#dependency('gvls-0.12')

install_data('gvls_plugin.py', install_dir: plugindir)

configure_file(
          input: 'gvls.plugin',
         output: 'gvls.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
