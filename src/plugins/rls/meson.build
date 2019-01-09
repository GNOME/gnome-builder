if get_option('plugin_rls')

install_data('rls_plugin.py', install_dir: plugindir)

configure_file(
          input: 'rls.plugin',
         output: 'rls.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
