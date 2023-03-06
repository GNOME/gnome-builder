if get_option('plugin_codeshot')

plugins_sources += files([
  'codeshot-plugin.c',
  'gbp-codeshot-editor-page-addin.c',
  'gbp-codeshot-window.c',
])

plugins_sources += gnome.compile_resources('codeshot-resources', 'codeshot.gresource.xml', c_name: 'gbp_codeshot')

endif
