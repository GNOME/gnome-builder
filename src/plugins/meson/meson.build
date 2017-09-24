if get_option('with_meson')

meson_resources = gnome.compile_resources(
  'builder-meson-resources',
  'meson.gresource.xml',
  c_name: 'gbp_meson',
)

gnome_builder_plugins_sources += meson_resources[0]

endif
