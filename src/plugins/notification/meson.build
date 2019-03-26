if get_option('plugin_notification')

plugins_sources += files([
  'notification-plugin.c',
  'ide-notification-addin.c',
])

plugin_notification_resources = gnome.compile_resources(
  'notification-resources',
  'notification.gresource.xml',
  c_name: 'gbp_notification',
)

plugins_sources += plugin_notification_resources

endif
