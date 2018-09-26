
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

# Appdata file.
appdata_file = i18n.merge_file(
  input: 'org.gnome.Builder.appdata.xml.in',
  output: 'org.gnome.Builder.appdata.xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  validate_args = ['validate-relax', appdata_file]
if not get_option('network_tests')
  validate_args += '--nonet'
endif
  test('Validate appstream file', appstream_util, args: validate_args)
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

subdir('fonts')
subdir('gsettings')
subdir('icons')
subdir('style-schemes')
