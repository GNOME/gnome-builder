plugins_sources += files([
  'gbp-menu-search-provider.c',
  'gbp-menu-search-result.c',
  'menu-search-plugin.c',
])

plugin_menu_search_resources = gnome.compile_resources(
  'menu-search-resources',
  'menu-search.gresource.xml',
  c_name: 'gbp_menu_search',
)

plugins_sources += plugin_menu_search_resources
