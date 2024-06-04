if get_option('plugin_gitlab')

plugins_sources += files([
  'gitlab-plugin.c',
  'gbp-gitlab-tweaks-addin.c',
  'gbp-gitlab-workbench-addin.c',
])

plugin_gitlab_resources = gnome.compile_resources(
  'gitlab-resources',
  'gitlab.gresource.xml',
  c_name: 'gbp_gitlab',
)

plugins_sources += plugin_gitlab_resources
plugins_deps += [dependency('gitlab-glib-1')]

install_data(['org.gnome.builder.gitlab.gschema.xml'], install_dir: schema_dir)

endif
