if get_option('with_java_langserv')

install_data('java_langserv_plugin.py', install_dir: plugindir)

configure_file(
          input: 'java-langserv.plugin',
         output: 'java-langserv.plugin',
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif