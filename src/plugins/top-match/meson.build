plugins_sources += files([
  'top-match-plugin.c',
  'gbp-top-match-completion-provider.c',
  'gbp-top-match-completion-proposal.c',
  'gbp-top-match-completion-filter.c',
  'gbp-top-match-completion-model.c',
])

plugin_top_match_resources = gnome.compile_resources(
  'gbp-top-match-resources',
  'top-match.gresource.xml',
  c_name: 'gbp_top_match',
)

plugins_sources += plugin_top_match_resources
