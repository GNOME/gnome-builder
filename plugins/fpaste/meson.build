if get_option('with_fpaste')

fpaste_datadir = join_paths(plugindatadir, 'fpaste_plugin')
fpaste_dir = join_paths(plugindir, 'fpaste_plugin')

install_data('fpaste.plugin', install_dir: plugindir)
install_data('fpaste_plugin/__init__.py', install_dir: fpaste_dir)
install_subdir('fpaste_plugin/gtk', install_dir: fpaste_datadir)

endif
