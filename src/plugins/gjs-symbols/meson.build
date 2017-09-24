if get_option('with_gjs_symbols')

gjs_symbols_resources = gnome.compile_resources(
  'gjs-symbols-resources',
  'gjs-symbols.gresource.xml',
  c_name: 'gjs_symbols',
)

gnome_builder_plugins_sources += gjs_symbols_resources[0]

endif
