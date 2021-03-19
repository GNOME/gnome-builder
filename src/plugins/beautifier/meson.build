if get_option('plugin_beautifier')

plugins_sources += files([
  'beautifier-plugin.c',
  'gb-beautifier-config.c',
  'gb-beautifier-helper.c',
  'gb-beautifier-process.c',
  'gb-beautifier-editor-addin.c',
])

plugin_beautifier_resources = gnome.compile_resources(
  'beautifier-resources',
  'beautifier.gresource.xml',
  c_name: 'gbp_beautifier',
)

beautifiers = [
  'autopep8',
  'tidy',
  'uncrustify',
  'xmllint',
]
# Only indicate the missing beautifiers at build time, but don't require them strongly.
foreach beautifier : beautifiers
  program = find_program(beautifier, required: false)
  if not program.found()
    message('Please install beautifier @0@ as runtime dependency'.format(beautifier))
  endif
endforeach

plugins_sources += plugin_beautifier_resources

endif
