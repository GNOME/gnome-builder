if get_option('with_html_completion')

html_completion_sources = [
	'ide-html-completion-provider.c',
	'ide-html-completion-provider.h',
]

shared_module('html-completion-plugin', html_completion_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'html-completion.plugin',
         output: 'html-completion.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
