if get_option('plugin_valgrind')

valgrind_resources = gnome.compile_resources(
  'valgrind_plugin',
  'valgrind.gresource.xml',
  gresource_bundle: true,
           install: true,
       install_dir: plugindir,
)

install_data('valgrind_plugin.py', install_dir: plugindir)

configure_file(
          input: 'valgrind.plugin',
         output: 'valgrind.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
