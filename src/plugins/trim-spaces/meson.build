plugins_sources += files([
  'trim-spaces-plugin.c',
  'gbp-trim-spaces-buffer-addin.c',
])

plugin_trim_spaces_resources = gnome.compile_resources(
  'gbp-trim-spaces-resources',
  'trim-spaces.gresource.xml',
  c_name: 'gbp_trim_spaces',
)

plugins_sources += plugin_trim_spaces_resources
