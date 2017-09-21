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

configure_file(
          input: 'notification.plugin',
         output: 'notification.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
