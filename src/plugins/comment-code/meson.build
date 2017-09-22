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
]

gnome_builder_plugins_sources += files(comment_code_sources)
gnome_builder_plugins_sources += comment_code_resources[0]

endif
