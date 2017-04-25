if get_option('with_notification')

notification_sources = [
  'ide-notification-plugin.c',
  'ide-notification-addin.c',
  'ide-notification-addin.h',
]

shared_module('notification-plugin', notification_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('notification.plugin', install_dir: plugindir)

endif
