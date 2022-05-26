if get_option('plugin_waf')

plugins_sources += files([
  'waf-plugin.c',
  'gbp-waf-build-system-discovery.c',
  'gbp-waf-build-system.c',
  'gbp-waf-build-target.c',
  'gbp-waf-build-target-provider.c',
  'gbp-waf-pipeline-addin.c',
  'gbp-waf-run-command-provider.c',
])

plugin_waf_resources = gnome.compile_resources(
  'waf-resources',
  'waf.gresource.xml',
  c_name: 'gbp_waf',
)

plugins_sources += plugin_waf_resources

endif
