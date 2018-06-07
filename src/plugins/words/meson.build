if get_option('with_words')

words_resources = gnome.compile_resources(
  'words-resources',
  'words.gresource.xml',
  c_name: 'gbp_words',
)

words_sources = [
  'words-plugin.c',
  'gbp-word-completion-provider.c',
  'gbp-word-proposal.c',
  'gbp-word-proposals.c',
]

gnome_builder_plugins_sources += files(words_sources)
gnome_builder_plugins_sources += words_resources[0]

endif
