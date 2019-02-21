if get_option('plugin_waf')

install_data('waf_plugin.py', install_dir: plugindir)

configure_file(
          input: 'waf.plugin',
         output: 'waf.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
