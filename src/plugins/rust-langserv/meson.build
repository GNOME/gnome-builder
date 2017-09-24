if get_option('with_rust_langserv')

rust_langserv_resources = gnome.compile_resources(
  'rust-langserv-resources',
  'rust-langserv.gresource.xml',
  c_name: 'gbp_rust_langserv',
)

gnome_builder_plugins_sources += rust_langserv_resources[0]

endif
