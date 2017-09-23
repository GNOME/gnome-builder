if get_option('with_meson')

install_data('meson_plugin.py', install_dir: plugindir)

configure_file(
          input: 'meson.plugin',
         output: 'meson.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
