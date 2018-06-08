if get_option('with_html_completion')

html_completion_resources = gnome.compile_resources(
  'html-completion-resources',
  'html-completion.gresource.xml',
  c_name: 'gbp_html_completion',
)

html_completion_sources = [
  'html-completion-plugin.c',
  'ide-html-completion-provider.c',
  'ide-html-proposal.c',
  'ide-html-proposals.c',
]

gnome_builder_plugins_sources += files(html_completion_sources)
gnome_builder_plugins_sources += html_completion_resources[0]

endif
