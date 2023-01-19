if get_option('plugin_codesearch')

subdir('libcodesearch')

plugins_sources += files([
  'codesearch-plugin.c',
  'gbp-codesearch-search-provider.c',
  'gbp-codesearch-workbench-addin.c',
])

plugin_codesearch_resources = gnome.compile_resources(
  'codesearch-resources',
  'codesearch.gresource.xml',
  c_name: 'gbp_codesearch',
)

plugins_sources += plugin_codesearch_resources
plugins_deps += [libcodesearch_static_dep]

endif
