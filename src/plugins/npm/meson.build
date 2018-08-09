if get_option('with_npm')

install_data('npm_plugin.py', install_dir: plugindir)

configure_file(
          input: 'npm.plugin',
         output: 'npm.plugin',
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif
