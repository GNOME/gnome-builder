if get_option('with_html_preview')

html_preview_resources = gnome.compile_resources(
  'html_preview',
  'html-preview.gresource.xml',
  c_name: 'gbp_html_preview',
)

gnome_builder_plugins_sources += html_preview_resources[0]

endif
