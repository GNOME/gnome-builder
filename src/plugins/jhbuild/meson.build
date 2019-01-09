if get_option('plugin_jhbuild')

install_data('jhbuild_plugin.py', install_dir: plugindir)

configure_file(
          input: 'jhbuild.plugin',
         output: 'jhbuild.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
