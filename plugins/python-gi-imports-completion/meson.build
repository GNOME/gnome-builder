if get_option('with_python_gi_imports_completion')

install_data('python_gi_imports_completion.py', install_dir: plugindir)

configure_file(
          input: 'python-gi-imports-completion.plugin',
         output: 'python-gi-imports-completion.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
