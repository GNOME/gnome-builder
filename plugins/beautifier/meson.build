if get_option('with_beautifier')

beautifier_resources = gnome.compile_resources(
  'gb-beautifier-resources',
  'gb-beautifier.gresource.xml',
  c_name: 'gb_beautifier'
)

beautifier_sources = [
  beautifier_resources,
  'gb-beautifier-config.c',
  'gb-beautifier-config.h',
  'gb-beautifier-helper.c',
  'gb-beautifier-helper.h',
  'gb-beautifier-plugin.c',
  'gb-beautifier-private.h',
  'gb-beautifier-process.c',
  'gb-beautifier-process.h',
  'gb-beautifier-workbench-addin.c',
  'gb-beautifier-workbench-addin.h',
]

beautifier_datadir = join_paths(get_option('prefix'), plugindatadir, 'beautifier')
shared_module('beautifier-plugin', beautifier_sources,
  dependencies: plugin_deps,
  c_args: '-DGB_BEAUTIFIER_PLUGIN_DATADIR="@0@"'.format(join_paths(beautifier_datadir, 'data')),
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('beautifier.plugin', install_dir: plugindir)
install_subdir('data', install_dir: beautifier_datadir)

endif
