doc_conf.set('PACKAGE', 'gnome-builder')
doc_conf.set('PACKAGE_NAME', 'gnome-builder')
doc_conf.set('PACKAGE_BUGREPORT', 'FIXME')
doc_conf.set('PACKAGE_STRING', 'gnome-builder')
doc_conf.set('PACKAGE_TARNAME', 'gnome-builder-' + meson.project_version())
doc_conf.set('PACKAGE_URL', 'FIXME')
doc_conf.set('PACKAGE_VERSION', meson.project_version())

configure_file(
  input: 'gtkdocentities.ent.in',
  output: 'gtkdocentities.ent',
  configuration: doc_conf,
)
