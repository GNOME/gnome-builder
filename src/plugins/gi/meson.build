if get_option('with_gi')

gi_resources = gnome.compile_resources(
  'gi-resources',
  'gi.gresource.xml',
  c_name: 'ide_gi'
)

gi_sources = [
  'gi-plugin.c',
  'ide-gi.c',
  'ide-gi.h',
  'ide-gi-objects.h',
  'ide-gi-blob.c',
  'ide-gi-blob.h',
  'ide-gi-complete.h',
  'ide-gi-complete-item.c',
  'ide-gi-complete-item.h',
  'ide-gi-crossref.c',
  'ide-gi-crossref.h',
  'ide-gi-file-builder.c',
  'ide-gi-file-builder.h',
  'ide-gi-file-builder-result.c',
  'ide-gi-file-builder-result.h',
  'ide-gi-formater.c',
  'ide-gi-formater.h',
  'ide-gi-helper.c',
  'ide-gi-helper.h',
  'ide-gi-index.c',
  'ide-gi-index.h',
  'ide-gi-index-private.h',
  'ide-gi-macros.h',
  'ide-gi-namespace.c',
  'ide-gi-namespace.h',
  'ide-gi-namespace-private.h',
  'ide-gi-parser.c',
  'ide-gi-parser.h',
  'ide-gi-parser-object.c',
  'ide-gi-parser-object.h',
  'ide-gi-parser-result.c',
  'ide-gi-parser-result.h',
  'ide-gi-pool.c',
  'ide-gi-pool.h',
  'ide-gi-repository.c',
  'ide-gi-repository.h',
  'ide-gi-require.c',
  'ide-gi-require.h',
  'ide-gi-service.c',
  'ide-gi-service.h',
  'ide-gi-types.h',
  'ide-gi-utils.c',
  'ide-gi-utils.h',
  'ide-gi-version.c',
  'ide-gi-version.h',
  'ide-gi-version-private.c',
  'ide-gi-version-private.h',
]

subdir('builder')
subdir('objects')
subdir('radix-tree')

gnome_builder_plugins_sources += files(gi_sources)
gnome_builder_plugins_sources += gi_resources[0]

endif
