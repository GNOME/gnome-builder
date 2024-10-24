subdir('appdata')
subdir('fonts')
subdir('gsettings')
subdir('icons')

# Desktop launcher and description file.
desktop_file_config = configuration_data()
desktop_file_config.set('app_id', app_id)
if app_id == 'org.gnome.Builder'
  desktop_file_config.set('X_FLATPAK_RENAMEDFROM', 'X-Flatpak-RenamedFrom=gnome-builder')
endif
desktop_file_in = configure_file(
          input: 'org.gnome.Builder.desktop.in.in',
         output: 'org.gnome.Builder.desktop.in',
  configuration: desktop_file_config,
)
desktop_file = i18n.merge_file(
        input: desktop_file_in,
       output: '@0@.desktop'.format(app_id),
         type: 'desktop',
       po_dir: '../po',
      install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils, args: [desktop_file])
endif

# D-Bus service file.
dbusconf = configuration_data()
dbusconf.set('app_id', app_id)
dbusconf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
configure_file(
          input: 'org.gnome.Builder.service.in',
         output: '@0@.service'.format(app_id),
  configuration: dbusconf,
        install: true,
    install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services'),
)
