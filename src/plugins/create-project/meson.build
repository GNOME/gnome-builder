plugins_sources += files([
  'create-project-plugin.c',
  'gbp-create-project-application-addin.c',
  'gbp-create-project-widget.c',
  'gbp-create-project-workspace-addin.c',
])

plugin_create_project_resources = gnome.compile_resources(
  'gbp-create-project-resources',
  'create-project.gresource.xml',
  c_name: 'gbp_create_project',
)

plugins_sources += plugin_create_project_resources
