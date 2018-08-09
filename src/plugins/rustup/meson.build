if get_option('with_rustup')

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
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif
