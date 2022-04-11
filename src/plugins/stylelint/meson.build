if get_option('plugin_stylelint')

install_data('stylelint_plugin.py', install_dir: plugindir)

configure_file(
          input: 'stylelint.plugin',
         output: 'stylelint.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
