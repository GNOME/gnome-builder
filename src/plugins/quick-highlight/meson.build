if get_option('with_quick_highlight')

quick_highlight_resources = gnome.compile_resources(
  'quick-highlight-resources',
  'quick-highlight.gresource.xml',
  c_name: 'gbp_quick_highlight',
)

quick_highlight_sources = [
  'gbp-quick-highlight-plugin.c',
  'gbp-quick-highlight-editor-view-addin.c',
  'gbp-quick-highlight-editor-view-addin.h',
  'gbp-quick-highlight-preferences.c',
  'gbp-quick-highlight-preferences.h',
]

gnome_builder_plugins_sources += files(quick_highlight_sources)
gnome_builder_plugins_sources += quick_highlight_resources[0]

endif
