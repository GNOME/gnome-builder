if get_option('plugin_html_completion')

plugins_sources += files([
  'html-completion-plugin.c',
  'ide-html-completion-provider.c',
  'ide-html-proposal.c',
  'ide-html-proposals.c',
])

plugin_html_completion_resources = gnome.compile_resources(
  'gbp-html-completion-resources',
  'html-completion.gresource.xml',
  c_name: 'gbp_html_completion',
)

plugins_sources += plugin_html_completion_resources

endif
