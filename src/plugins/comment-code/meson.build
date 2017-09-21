if get_option('with_comment_code')

comment_code_resources = gnome.compile_resources(
  'gbp-comment-code-resources',
  'gbp-comment-code.gresource.xml',
  c_name: 'gbp_comment_code',
)

comment_code_sources = [
  'gbp-comment-code-plugin.c',
  'gbp-comment-code-view-addin.c',
  'gbp-comment-code-view-addin.h',
  comment_code_resources[0],
]

shared_module('comment-code-plugin', comment_code_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'comment-code.plugin',
         output: 'comment-code.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
