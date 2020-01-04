if get_option('plugin_vala')

install_data('vala_pack_plugin.py', install_dir: plugindir)

configure_file(
          input: 'vala-pack.plugin',
         output: 'vala-pack.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
