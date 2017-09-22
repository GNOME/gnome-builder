if get_option('with_code_index')

code_index_resources = gnome.compile_resources(    
  'ide-code-index-resources',                      
  'code-index.gresource.xml',                      
  c_name: 'ide_code_index',                        
)                                           

code_index_sources = [
  'code-index-plugin.c',
  'ide-code-index-builder.c',
  'ide-code-index-builder.h',
  'ide-code-index-index.c',
  'ide-code-index-index.h',
  'ide-code-index-search-provider.c',
  'ide-code-index-search-provider.h',
  'ide-code-index-search-result.c',
  'ide-code-index-search-result.h',
  'ide-code-index-service.c',
  'ide-code-index-service.h',
  'ide-code-index-symbol-resolver.c',
  'ide-code-index-symbol-resolver.h',
  'ide-persistent-map-builder.c',
  'ide-persistent-map-builder.h',
  'ide-persistent-map.c',
  'ide-persistent-map.h',
]

gnome_builder_plugins_sources += files(code_index_sources)
gnome_builder_plugins_sources += code_index_resources[0]

endif
