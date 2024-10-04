icondir = join_paths(get_option('datadir'), 'icons/hicolor')
pkgicondir = join_paths(get_option('datadir'), 'gnome-builder/icons/hicolor')

install_subdir('hicolor/16x16', install_dir: pkgicondir)
install_subdir('hicolor/32x32', install_dir: pkgicondir)
install_subdir('hicolor/48x48', install_dir: pkgicondir)
install_subdir('hicolor/64x64', install_dir: pkgicondir)
install_subdir('hicolor/256x256', install_dir: pkgicondir)
install_subdir('hicolor/scalable', install_dir: pkgicondir)
install_subdir('hicolor/symbolic', install_dir: pkgicondir)

app_icon = '@0@.svg'.format(app_id)

app_icons = [ 'hicolor/scalable/apps/org.gnome.Builder.svg' ]
symbolic_icons = [ 'hicolor/symbolic/apps/org.gnome.Builder-symbolic.svg' ]

if get_option('development')
  app_icons += [ 'hicolor/scalable/apps/@0@.svg'.format(app_id) ]
  symbolic_icons += [ 'hicolor/symbolic/apps/@0@-symbolic.svg'.format(app_id) ]
endif

install_data(app_icons, install_dir: join_paths(icondir, 'scalable/apps'))
install_data(symbolic_icons, install_dir: join_paths(icondir, 'symbolic/apps'))