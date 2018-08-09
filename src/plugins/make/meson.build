if get_option('with_make')

make_resources = gnome.compile_resources(
  'make_plugin',
  'make.gresource.xml',
  gresource_bundle: true,
           install: true,
       install_dir: plugindir,
)

install_data('make_plugin.py', install_dir: plugindir)

configure_file(
          input: 'make.plugin',
         output: 'make.plugin',
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif
