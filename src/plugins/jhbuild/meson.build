if get_option('with_jhbuild')

install_data('jhbuild_plugin.py', install_dir: plugindir)

configure_file(
          input: 'jhbuild.plugin',
         output: 'jhbuild.plugin',
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif
