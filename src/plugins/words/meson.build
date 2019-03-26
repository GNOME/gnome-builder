if get_option('plugin_words')

plugins_sources += files([
  'words-plugin.c',
  'gbp-word-completion-provider.c',
  'gbp-word-proposal.c',
  'gbp-word-proposals.c',
])

plugin_words_resources = gnome.compile_resources(
  'words-resources',
  'words.gresource.xml',
  c_name: 'gbp_words',
)

plugins_sources += plugin_words_resources

endif
