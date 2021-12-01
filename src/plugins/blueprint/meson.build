if get_option('plugin_blueprint')

install_data('blueprint_plugin.py', install_dir: plugindir)

configure_file(
          input: 'blueprint.plugin',
         output: 'blueprint.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

install_data('blueprint.lang',
  install_dir: join_paths(get_option('datadir'), 'gtksourceview-4' / 'language-specs'))

endif
