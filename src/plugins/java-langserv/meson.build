if get_option('plugin_java_langserv')

install_data('java_langserver_plugin.py', install_dir: plugindir)

configure_file(
          input: 'java-langserv.plugin',
         output: 'java-langserv.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
