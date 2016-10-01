if get_option('with_html_preview')

html_preview_datadir = join_paths(plugindatadir, 'html_preview_plugin')
html_preview_dir = join_paths(plugindir, 'html_preview_plugin')

install_data('html-preview.plugin', install_dir: plugindir)
install_data('html_preview_plugin/__init__.py', install_dir: html_preview_dir)
install_data([
  'html_preview_plugin/markdown-view.js',
  'html_preview_plugin/markdown.css',
  'html_preview_plugin/marked.js',
], install_dir: html_preview_datadir)
install_subdir('html_preview_plugin/gtk', install_dir: html_preview_datadir)

endif
