if get_option('plugin_manuals')

if not get_option('webkit').enabled()
  error('-Dwebkit=enabled is required for manuals plugin')
endif

plugins_sources += files([
  'gbp-manuals-application-addin.c',
  'gbp-manuals-page.c',
  'gbp-manuals-panel.c',
  'gbp-manuals-pathbar.c',
  'gbp-manuals-search-provider.c',
  'gbp-manuals-search-result.c',
  'gbp-manuals-tree-addin.c',
  'gbp-manuals-workspace-addin.c',
  'manuals-book.c',
  'manuals-devhelp-importer.c',
  'manuals-gio.c',
  'manuals-heading.c',
  'manuals-importer.c',
  'manuals-jhbuild-importer.c',
  'manuals-job.c',
  'manuals-keyword.c',
  'manuals-navigatable.c',
  'manuals-navigatable-model.c',
  'manuals-path-button.c',
  'manuals-path-element.c',
  'manuals-path-model.c',
  'manuals-plugin.c',
  'manuals-progress.c',
  'manuals-purge-missing.c',
  'manuals-repository.c',
  'manuals-sdk.c',
  'manuals-search-query.c',
  'manuals-search-model.c',
  'manuals-search-result.c',
  'manuals-system-importer.c',
  'manuals-tag.c',
  'manuals-utils.c',
])

if get_option('plugin_flatpak')
  plugins_sources += files([
    'manuals-flatpak-runtime.c',
    'manuals-flatpak-importer.c',
  ])
endif

plugin_manuals_resources = gnome.compile_resources(
  'manuals-resources',
  'manuals.gresource.xml',
  c_name: 'gbp_manuals',
)

plugins_deps += [
  dependency('gom-1.0'),
]

plugins_sources += plugin_manuals_resources

endif
