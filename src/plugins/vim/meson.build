plugins_sources += files([
  'vim-plugin.c',
  'gbp-vim-editor-page-addin.c',
  'gbp-vim-tweaks-addin.c',
  'gbp-vim-workspace-addin.c',
])

plugin_vim_resources = gnome.compile_resources(
  'vim-resources',
  'vim.gresource.xml',
  c_name: 'gbp_vim',
)

plugins_sources += plugin_vim_resources
