if get_option('plugin_golang')

install_data('org.gnome.builder.plugins.golang.gschema.xml',
  install_dir: schema_dir)

golang_resources = gnome.compile_resources(
  'ide-golang-resources',
  'golang.gresource.xml',
  c_name: 'ide_golang',
)

golang_sources = [
  'golang-plugin.c',
  'ide-golang-application-addin.c',
  'ide-golang-application-addin.h',
  'ide-golang-build-system.c',
  'ide-golang-build-system.h',
  'ide-golang-build-system-discovery.c',
  'ide-golang-build-system-discovery.h',
  'ide-golang-go-stage.c',
  'ide-golang-go-stage.h',
  'ide-golang-pipeline-addin.c',
  'ide-golang-pipeline-addin.h',
  'ide-golang-preferences-addin.c',
  'ide-golang-preferences-addin.h'
]

plugins_sources += files(golang_sources)
plugins_sources += golang_resources[0]

endif
