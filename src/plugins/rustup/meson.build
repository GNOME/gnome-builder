if get_option('plugin_rustup')

rustup_resources = gnome.compile_resources(
  'rustup_plugin',
  'rustup.gresource.xml',
  gresource_bundle: true,
           install: true,
       install_dir: plugindir,
)

install_data('rustup_plugin.py', install_dir: plugindir)

configure_file(
          input: 'rustup.plugin',
         output: 'rustup.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
