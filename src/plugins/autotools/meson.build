if get_option('with_autotools')

autotools_resources = gnome.compile_resources(    
  'ide-autotools-resources',                      
  'autotools.gresource.xml',                      
  c_name: 'ide_autotools',                        
)                                           

autotools_sources = [
  'autotools-plugin.c',
  'ide-autotools-application-addin.c',
  'ide-autotools-application-addin.h',
  'ide-autotools-autogen-stage.c',
  'ide-autotools-autogen-stage.h',
  'ide-autotools-build-system.c',
  'ide-autotools-build-system.h',
  'ide-autotools-build-target.c',
  'ide-autotools-build-target.h',
  'ide-autotools-make-stage.c',
  'ide-autotools-make-stage.h',
  'ide-autotools-makecache-stage.c',
  'ide-autotools-makecache-stage.h',
  'ide-autotools-pipeline-addin.c',
  'ide-autotools-pipeline-addin.h',
  'ide-makecache.c',
  'ide-makecache.h',
  'ide-makecache-target.c',
  'ide-makecache-target.h',
]

gnome_builder_plugins_sources += files(autotools_sources)       
gnome_builder_plugins_sources += autotools_resources[0]         

endif
