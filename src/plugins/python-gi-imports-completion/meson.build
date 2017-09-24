if get_option('with_python_gi_imports_completion')

python_gi_imports_completion_resources = gnome.compile_resources(
  'python-gi-imports-completion-resources',
  'python-gi-imports-completion.gresource.xml',
  c_name: 'gbp_python_gi_imports_completion',
)

gnome_builder_plugins_sources += python_gi_imports_completion_resources[0]

endif
