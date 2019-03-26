plugins_sources += files([
  'comment-code-plugin.c',
  'gbp-comment-code-editor-page-addin.c',
])

plugin_comment_code_resources = gnome.compile_resources(
  'gbp-comment-code-resources',
  'comment-code.gresource.xml',
  c_name: 'gbp_comment_code',
)

plugins_sources += plugin_comment_code_resources
