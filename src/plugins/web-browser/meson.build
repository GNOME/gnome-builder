if get_option('webkit').enabled()

plugins_sources += files([
  'web-browser-plugin.c',
  'gbp-web-browser-workbench-addin.c',
  'gbp-web-browser-workspace-addin.c',
])

plugin_web_browser_resources = gnome.compile_resources(
  'gbp-web-browser-resources',
  'web-browser.gresource.xml',
  c_name: 'gbp_web_browser',
)

plugins_sources += plugin_web_browser_resources

endif
