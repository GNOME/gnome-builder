if get_option('with_docs')

doc_conf = configuration_data()
doc_conf.set('LIBIDE_VERSION', libide_api_version)

subdir('xml')

doc_version_xml = configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: doc_conf,
)

glib_prefix = libgio_dep.get_pkgconfig_variable('prefix')
glib_doc_dir = join_paths(glib_prefix, 'share/gtk-doc/html')
doc_dir_arg = '--extra-dir=' + glib_doc_dir

gnome.gtkdoc('libide',
  main_sgml: 'libide-docs.sgml',
  src_dir: join_paths(meson.source_root(), 'libide'),
  dependencies: libide_dep,
  install: true,
  scan_args: [
    '--ignore-headers=config.h',
  ],
  fixxref_args: [
    '--html-dir=' + join_paths(get_option('prefix'), get_option('datadir'), 'gtk-doc/html'),
    join_paths(doc_dir_arg, 'glib'),
    join_paths(doc_dir_arg, 'gmodule'),
    join_paths(doc_dir_arg, 'gobject'),
    join_paths(doc_dir_arg, 'gio'),
    join_paths(doc_dir_arg, 'gtk'),
    join_paths(doc_dir_arg, 'gtksourceview-3.0'),
  ],
  content_files: 'xml/tree_index.sgml',
)

endif
