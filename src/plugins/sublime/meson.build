plugins_sources += files([
  'sublime-plugin.c',
  'gbp-sublime-preferences-addin.c',
])

plugin_sublime_resources = gnome.compile_resources(
  'sublime-resources',
  'sublime.gresource.xml',
  c_name: 'gbp_sublime',
)

plugins_sources += plugin_sublime_resources
