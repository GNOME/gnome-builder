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
  create_project_resources[0],
]

shared_module('create-project-plugin', create_project_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'create-project.plugin',
         output: 'create-project.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
