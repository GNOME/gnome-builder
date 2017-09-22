if get_option('with_notification')

notification_resources = gnome.compile_resources(
  'notification-resources',
  'notification.gresource.xml',
  c_name: 'ide_notification',
)

notification_sources = [
  'ide-notification-plugin.c',
  'ide-notification-addin.c',
  'ide-notification-addin.h',
]

gnome_builder_plugins_sources += files(notification_sources)
gnome_builder_plugins_sources += notification_resources[0]

endif
