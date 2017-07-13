if get_option('with_devhelp')

devhelp_resources = gnome.compile_resources(
  'gbp-devhelp-resources',
  'gbp-devhelp-resources.gresource.xml',
  c_name: 'gbp_devhelp',
)

devhelp_sources = [
  'gbp-devhelp-menu-button.c',
  'gbp-devhelp-menu-button.h',
  'gbp-devhelp-layout-stack-addin.c',
  'gbp-devhelp-layout-stack-addin.h',

  'gbp-devhelp-editor-view-addin.c',
  'gbp-devhelp-editor-view-addin.h',
  'gbp-devhelp-panel.c',
  'gbp-devhelp-panel.h',
  'gbp-devhelp-plugin.c',
  'gbp-devhelp-search.c',
  'gbp-devhelp-search.h',
  'gbp-devhelp-search-private.h',
  'gbp-devhelp-view.c',
  'gbp-devhelp-view.h',
  devhelp_resources[0],
]

devhelp_deps = plugin_deps + [
  dependency('libdevhelp-3.0', version: '>=3.25.1'),
  libgd_dep,
]

shared_module('devhelp-plugin', devhelp_sources,
  dependencies: devhelp_deps,
     link_args: plugin_link_args,
  link_depends: plugin_link_deps,
       install: true,
   install_dir: plugindir,
)

configure_file(
          input: 'devhelp.plugin',
         output: 'devhelp.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
