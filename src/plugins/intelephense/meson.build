if get_option('plugin_intelephense')

install_data('intelephense.py', install_dir: plugindir)

configure_file(
          input: 'intelephense.plugin',
         output: 'intelephense.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
