if get_option('plugin_rstcheck')

install_data('rstcheck_plugin.py', install_dir: plugindir)

configure_file(
          input: 'rstcheck.plugin',
         output: 'rstcheck.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
