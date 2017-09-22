if get_option('with_spellcheck')

spellcheck_resources = gnome.compile_resources(
  'gbp-spell-resources',
  'spellcheck.gresource.xml',
  c_name: 'gbp_spell',
)

spellcheck_sources = [
  'spellcheck-plugin.c',
  'gbp-spell-buffer-addin.c',
  'gbp-spell-buffer-addin.h',
  'gbp-spell-dict.c',
  'gbp-spell-dict.h',
  'gbp-spell-editor-addin.c',
  'gbp-spell-editor-addin.h',
  'gbp-spell-editor-view-addin.c',
  'gbp-spell-editor-view-addin.h',
  'gbp-spell-language-popover.c',
  'gbp-spell-language-popover.h',
  'gbp-spell-navigator.c',
  'gbp-spell-navigator.h',
  'gbp-spell-utils.c',
  'gbp-spell-utils.h',
  'gbp-spell-widget.c',
  'gbp-spell-widget.h',
  'gbp-spell-widget-actions.c',
]

gnome_builder_plugins_deps += [
  dependency('gspell-1', version: '>= 1.2.0'),
  dependency('enchant'),
]

gnome_builder_plugins_sources += files(spellcheck_sources)
gnome_builder_plugins_sources += spellcheck_resources[0]

endif
