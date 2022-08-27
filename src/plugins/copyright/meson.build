if get_option('plugin_copyright')

plugins_sources += files(
  'copyright-plugin.c',
  'gbp-copyright-buffer-addin.c',
  'gbp-copyright-tweaks-addin.c',
  'gbp-copyright-util.c',
)

plugins_sources += gnome.compile_resources(
  'copyright-resources',
  'copyright.gresource.xml',
  c_name: 'gbp_copyright',
)

install_data('org.gnome.builder.copyright.gschema.xml', install_dir: schema_dir)

test_copyright = executable('test-copyright', ['test-copyright.c', 'gbp-copyright-util.c'],
  dependencies: [ libglib_dep ],
)
test('test-copyright', test_copyright)

endif
