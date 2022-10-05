if get_option('plugin_lua_language_server')

plugins_sources += files([
  'lua-language-server-plugin.c',
  'gbp-lua-code-action-provider.c',
  'gbp-lua-completion-provider.c',
  'gbp-lua-diagnostic-provider.c',
  'gbp-lua-formatter.c',
  'gbp-lua-highlighter.c',
  'gbp-lua-hover-provider.c',
  'gbp-lua-rename-provider.c',
  'gbp-lua-symbol-resolver.c',
  'gbp-lua-service.c',
])

plugin_lua_resources = gnome.compile_resources(
  'lua-language-server-resources',
  'lua-language-server.gresource.xml',
  c_name: 'gbp_lua',
)

plugins_sources += plugin_lua_resources

endif
