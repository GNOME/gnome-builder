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

shared_module('beautifier_plugin', beautifier_sources,
  dependencies: [libpeas_dep, libide_plugin_dep],
     link_args: [gnome_builder_plugins_link_args],
  link_depends: [gnome_builder_plugins_link_deps],
       install: true,
   install_dir: plugindir,
)

configure_file(
          input: 'beautifier.plugin',
         output: 'beautifier.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

# TODO: Make the beautifier work with embedded resources so that
#       we can statically link the plugin into libgnome-builder-plugins.so
beautifier_datadir = join_paths(get_option('prefix'), plugindatadir, 'beautifier_plugin')
install_subdir('beautifier_plugin/data', install_dir: beautifier_datadir)
install_subdir('beautifier_plugin/internal', install_dir: beautifier_datadir)

endif
