if get_option('with_maven')

install_data('maven_plugin.py', install_dir: plugindir)

configure_file(
          input: 'maven.plugin',
         output: 'maven.plugin',
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif
