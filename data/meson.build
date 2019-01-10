subdir('appdata')
subdir('fonts')
subdir('gsettings')
subdir('icons')
subdir('style-schemes')

# Desktop launcher and description file.
desktop_file = i18n.merge_file(
  input: 'org.gnome.Builder.desktop.in.in',
  output: 'org.gnome.Builder.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils,
    args: [desktop_file]
  )
endif

# D-Bus service file.
dbusconf = configuration_data()
dbusconf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
configure_file(
	input: 'org.gnome.Builder.service.in',
	output: 'org.gnome.Builder.service',
	configuration: dbusconf,
	install: true,
	install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services'),
)
