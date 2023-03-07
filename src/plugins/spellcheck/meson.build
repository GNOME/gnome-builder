if get_option('plugin_spellcheck')

plugins_sources += files([
  'editor-empty-spell-provider.c',
  'editor-enchant-spell-language.c',
  'editor-enchant-spell-provider.c',
  'editor-spell-checker.c',
  'editor-spell-cursor.c',
  'editor-spell-language.c',
  'editor-spell-language-info.c',
  'editor-spell-menu.c',
  'editor-spell-provider.c',
  'editor-text-buffer-spell-adapter.c',
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
  dependency('enchant-2'),
  dependency('icu-uc'),
]

plugins_sources += plugin_spellcheck_resources

test_spell_cursor_sources = [
  'test-spell-cursor.c',
]
test_spell_cursor = executable('test-spell-cursor', test_spell_cursor_sources,
         dependencies: [libgtk_dep, libgtksource_dep],
  include_directories: [
    include_directories('.'),
    include_directories('../../libide/code'),
  ]
)
test('test-spell-cursor', test_spell_cursor)

endif
