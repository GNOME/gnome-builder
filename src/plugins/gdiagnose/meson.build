if get_option('plugin_gdiagnose')

plugins_sources += files([
  'gdiagnose-plugin.c',
  'gbp-gdiagnose-chainups.c',
  'gbp-gdiagnose-diagnostic-provider.c',
])

plugin_gdiagnose_resources = gnome.compile_resources(
  'gdiagnose-resources',
  'gdiagnose.gresource.xml',
  c_name: 'gbp_gdiagnose',
)

plugins_sources += plugin_gdiagnose_resources

endif
