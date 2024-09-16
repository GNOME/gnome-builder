if get_option('plugin_spellcheck')

plugins_sources += files([
  'gbp-spell-buffer-addin.c',
  'gbp-spell-editor-page-addin.c',
  'spellcheck-plugin.c',
])

plugin_spellcheck_resources = gnome.compile_resources(
  'spellcheck-resources',
  'spellcheck.gresource.xml',
  c_name: 'gbp_spellcheck',
)

plugins_deps += [
  dependency('libspelling-1', version: '>= 0.3'),
]

plugins_sources += plugin_spellcheck_resources

endif
