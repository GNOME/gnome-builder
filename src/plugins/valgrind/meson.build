if get_option('with_valgrind')

install_data('valgrind_plugin.py', install_dir: plugindir)

valgrind_resources = gnome.compile_resources(
  'valgrind_plugin',
  'valgrind-plugin.gresource.xml',

  gresource_bundle: true,
           install: true,
       install_dir: plugindir,
)

configure_file(
          input: 'valgrind.plugin',
         output: 'valgrind.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
