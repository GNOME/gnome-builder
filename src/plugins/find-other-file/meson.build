plugins_sources += files([
  'find-other-file-plugin.c',
  'gbp-simple-similar-file-locator.c',
  'gbp-find-other-file-browser.c',
  'gbp-find-other-file-popover.c',
  'gbp-find-other-file-workspace-addin.c',
  'gbp-found-file.c',
])

plugin_find_other_file_resources = gnome.compile_resources(
  'find-other-file-resources',
  'find-other-file.gresource.xml',
  c_name: 'gbp_find_other_file',
)

plugins_sources += plugin_find_other_file_resources
