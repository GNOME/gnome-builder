if get_option('with_meson')

meson_resources = gnome.compile_resources(    
  'gbp-meson-resources',                      
  'meson.gresource.xml',                      
  c_name: 'gbp_meson',                        
)                                           

meson_sources = [
  'meson-plugin.c',
  'gbp-meson-build-system.c',
  'gbp-meson-build-system.h',
  'gbp-meson-build-target.c',
  'gbp-meson-build-target.h',
  'gbp-meson-build-target-provider.c',
  'gbp-meson-build-target-provider.h',
  'gbp-meson-pipeline-addin.c',
  'gbp-meson-pipeline-addin.h',
  'gbp-meson-test-provider.c',
  'gbp-meson-test-provider.h',
  'gbp-meson-test.c',
  'gbp-meson-test.h',
]

gnome_builder_plugins_sources += files(meson_sources)       
gnome_builder_plugins_sources += meson_resources[0]         

endif
