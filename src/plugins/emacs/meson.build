plugins_sources += files([
  'emacs-plugin.c',
  'gbp-emacs-preferences-addin.c',
])

plugin_emacs_resources = gnome.compile_resources(
  'emacs-resources',
  'emacs.gresource.xml',
  c_name: 'gbp_emacs',
)

plugins_sources += plugin_emacs_resources
