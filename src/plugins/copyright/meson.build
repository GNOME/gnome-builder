if get_option('plugin_copyright')
  plugins_sources += files(
    'copyright-plugin.c',
    'gbp-copyright-buffer-addin.c',
    'gbp-copyright-preferences-addin.c',
    'gbp-copyright-util.c',
  )

  install_data('org.gnome.builder.plugins.copyright.gschema.xml', install_dir: schema_dir)

  test_copyright = executable('test-copyright',
    ['test-copyright.c', 'gbp-copyright-util.c'],
    dependencies: [ libglib_dep ],
  )
  test('test-copyright', test_copyright)
endif
