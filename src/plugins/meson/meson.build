if get_option('with_meson')

install_data('meson_plugin/__init__.py', install_dir: join_paths(plugindir, 'meson_plugin'))

configure_file(
          input: 'meson.plugin',
         output: 'meson.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
