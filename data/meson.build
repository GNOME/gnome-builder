msgfmt = find_program('msgfmt')
podir = join_paths(meson.source_root(), 'po')

# Desktop launcher and description file.
custom_target(
  'desktop-file',
  input: 'org.gnome.Builder.desktop.in.in',
  output: 'org.gnome.Builder.desktop',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
  command: [msgfmt, '--desktop', '--template', '@INPUT@',
    '-d', podir, '-o', '@OUTPUT@'],
)

# Appdata file.
custom_target(
  'appdata-file',
  input: 'org.gnome.Builder.appdata.xml.in',
  output: 'org.gnome.Builder.appdata.xml',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'appdata'),
  command: [msgfmt, '--xml', '--template', '@INPUT@',
    '-d', podir, '-o', '@OUTPUT@'],
)

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
