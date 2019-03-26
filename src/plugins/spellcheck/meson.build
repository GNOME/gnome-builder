if get_option('plugin_spellcheck')

plugins_sources += files([
  'gbp-spell-buffer-addin.c',
  'gbp-spell-dict.c',
  'gbp-spell-editor-addin.c',
  'gbp-spell-editor-page-addin.c',
  'gbp-spell-language-popover.c',
  'gbp-spell-navigator.c',
  'gbp-spell-utils.c',
  'gbp-spell-widget-actions.c',
  'gbp-spell-widget.c',
  'spellcheck-plugin.c',
])

plugin_spellcheck_resources = gnome.compile_resources(
  'spellcheck-resources',
  'spellcheck.gresource.xml',
  c_name: 'gbp_spellcheck',
)

plugins_deps += [
  dependency('gspell-1', version: '>= 1.2.0'),
  dependency('enchant-2'),
]

plugins_sources += plugin_spellcheck_resources

endif
