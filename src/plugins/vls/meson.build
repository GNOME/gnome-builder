if get_option('plugin_vls')

install_data('vala_langserv.py', install_dir: plugindir)

configure_file(
          input: 'vala_langserv.plugin',
         output: 'vala_langserv.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
