if get_option('with_beautifier')

beautifier_resources = gnome.compile_resources(
  'gb-beautifier-resources',
  'gb-beautifier.gresource.xml',
  c_name: 'gb_beautifier'
)

beautifier_sources = [
  'gb-beautifier-config.c',
  'gb-beautifier-config.h',
  'gb-beautifier-helper.c',
  'gb-beautifier-helper.h',
  'gb-beautifier-plugin.c',
  'gb-beautifier-private.h',
  'gb-beautifier-process.c',
  'gb-beautifier-process.h',
  'gb-beautifier-editor-addin.c',
  'gb-beautifier-editor-addin.h',
]

gnome_builder_plugins_sources += files(beautifier_sources)
gnome_builder_plugins_sources += beautifier_resources[0]

beautifier_datadir = join_paths(get_option('prefix'), plugindatadir, 'beautifier_plugin')
install_subdir('beautifier_plugin/data', install_dir: beautifier_datadir)
install_subdir('beautifier_plugin/internal', install_dir: beautifier_datadir)

endif
