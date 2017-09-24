if get_option('with_find_other_file')

find_other_file_resources = gnome.compile_resources(
  'find-other-file-resources',
  'find-other-file.gresource.xml',
  c_name: 'gbp_find_other_file',
)

gnome_builder_plugins_sources += find_other_file_resources[0]

endif
