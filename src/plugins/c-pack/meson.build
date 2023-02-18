if get_option('plugin_c_pack')

plugins_sources += files([
  'c-pack-plugin.c',
  'c-parse-helper.c',
  'cpack-editor-page-addin.c',
  'ide-c-indenter.c',
  'hdr-format.c',
])

plugin_c_pack_resources = gnome.compile_resources(
  'c-pack-resources',
  'c-pack.gresource.xml',
  c_name: 'ide_c',
)

plugins_sources += plugin_c_pack_resources

test_cpack = executable('test-cpack',
  'test-cpack.c', 'c-parse-helper.c',
        c_args: test_cflags,
  dependencies: [ libide_projects_dep ],
)
test('test-cpack', test_cpack, env: test_env)

test_hdr_format = executable('test-hdr-format',
  'test-hdr-format.c', 'c-parse-helper.c',
        c_args: test_cflags,
  dependencies: [ libide_sourceview_dep ],
)

endif
