recent_resources = gnome.compile_resources(
  'recent-resources',
  'recent.gresource.xml',
  c_name: 'gbp_recent',
)

recent_sources = [
  'recent-plugin.c',
  'gbp-recent-project-row.c',
  'gbp-recent-project-row.h',
  'gbp-recent-section.c',
  'gbp-recent-section.h',
]

gnome_builder_plugins_sources += files(recent_sources)
gnome_builder_plugins_sources += recent_resources[0]
