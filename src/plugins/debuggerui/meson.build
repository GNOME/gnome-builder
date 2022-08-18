plugins_sources += files([
  'debuggerui-plugin.c',
  'gbp-debuggerui-tweaks-addin.c',
  'gbp-debugger-tool.c',
  'ide-debugger-breakpoints-view.c',
  'ide-debugger-controls.c',
  'ide-debugger-disassembly-view.c',
  'ide-debugger-workspace-addin.c',
  'ide-debugger-hover-controls.c',
  'ide-debugger-hover-provider.c',
  'ide-debugger-libraries-view.c',
  'ide-debugger-locals-view.c',
  'ide-debugger-registers-view.c',
  'ide-debugger-threads-view.c',
  'ide-debugger-log-view.c',
])

plugin_debuggerui_resources = gnome.compile_resources(
  'gbp-debuggerui-resources',
  'debuggerui.gresource.xml',
  c_name: 'gbp_debuggerui',
)

plugins_sources += plugin_debuggerui_resources
