if get_option('with_html_preview')

html_preview_resources = gnome.compile_resources(
  'html_preview',
  'html-preview.gresource.xml',
  gresource_bundle: true,
           install: true,
       install_dir: plugindir,
)

install_data('html_preview.py', install_dir: plugindir)

configure_file(
          input: 'html-preview.plugin',
         output: 'html-preview.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
