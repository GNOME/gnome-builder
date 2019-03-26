plugins_sources += files([
  'vim-plugin.c',
  'gb-vim.c',
  'gbp-vim-command.c',
  'gbp-vim-command-provider.c',
  'gbp-vim-preferences-addin.c',
])

plugin_vim_resources = gnome.compile_resources(
  'vim-resources',
  'vim.gresource.xml',
  c_name: 'gbp_vim',
)

plugins_sources += plugin_vim_resources
