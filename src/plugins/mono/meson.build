if get_option('with_mono')

install_data('mono_plugin.py', install_dir: plugindir)

configure_file(
          input: 'mono.plugin',
         output: 'mono.plugin',
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif
