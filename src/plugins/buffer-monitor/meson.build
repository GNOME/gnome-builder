plugins_sources += files([
  'buffer-monitor-plugin.c',
  'gbp-buffer-monitor-buffer-addin.c',
])

plugin_buffer_monitor_resources = gnome.compile_resources(
  'gbp-buffer-monitor-resources',
  'buffer-monitor.gresource.xml',
  c_name: 'gbp_buffer_monitor',
)

plugins_sources += plugin_buffer_monitor_resources
