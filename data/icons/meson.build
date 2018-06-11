icondir = join_paths(get_option('datadir'), 'icons/hicolor')

install_subdir('hicolor/16x16', install_dir: icondir)
install_subdir('hicolor/24x24', install_dir: icondir)
install_subdir('hicolor/32x32', install_dir: icondir)
install_subdir('hicolor/48x48', install_dir: icondir)
install_subdir('hicolor/64x64', install_dir: icondir)
install_subdir('hicolor/128x128', install_dir: icondir)
install_subdir('hicolor/256x256', install_dir: icondir)
install_subdir('hicolor/512x512', install_dir: icondir)
install_subdir('hicolor/symbolic', install_dir: icondir)

# Sadly some of these directories have unrelated files
# TODO: Make this cleaner upstream

install_data('hicolor/32x32/apps/org.gnome.Builder.png',   install_dir: join_paths(icondir, '32x32/apps'))
install_data('hicolor/48x48/apps/org.gnome.Builder.png',   install_dir: join_paths(icondir, '48x48/apps'))
install_data('hicolor/256x256/apps/org.gnome.Builder.png', install_dir: join_paths(icondir, '256x256/apps'))

