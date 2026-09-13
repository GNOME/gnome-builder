if get_option('plugin_acp')

plugins_sources += files([
  'acp-plugin.c',
  'gbp-acp-client.c',
  'gbp-acp-jsonrpc.c',
  'gbp-acp-terminal.c',
  'gbp-acp-workspace-addin.c',
  'gbp-acp-pane.c',
  'gbp-acp-message-row.c',
  'gbp-acp-tool-call-row.c',
  'gbp-acp-permission-row.c',
  'gbp-acp-diff-view.c',
])

plugin_acp_resources = gnome.compile_resources(
  'acp-resources',
  'acp.gresource.xml',
  c_name: 'gbp_acp',
)

plugins_sources += plugin_acp_resources

endif
