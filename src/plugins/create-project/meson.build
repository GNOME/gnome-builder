if get_option('with_create_project')

create_project_resources = gnome.compile_resources(
  'gbp-create-project-resources',
  'gbp-create-project.gresource.xml',
  c_name: 'gbp_create_project',
)

create_project_sources = [
  'gbp-create-project-genesis-addin.c',
  'gbp-create-project-genesis-addin.h',
  'gbp-create-project-plugin.c',
  'gbp-create-project-template-icon.c',
  'gbp-create-project-template-icon.h',
  'gbp-create-project-tool.c',
  'gbp-create-project-tool.h',
  'gbp-create-project-widget.c',
  'gbp-create-project-widget.h',
]

gnome_builder_plugins_sources += files(create_project_sources)
gnome_builder_plugins_sources += create_project_resources[0]

endif
