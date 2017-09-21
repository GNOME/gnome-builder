debugger_headers = [
  'ide-debug-manager.h',
  'ide-debugger.h',
  'ide-debugger-breakpoint.h',
  'ide-debugger-breakpoints.h',
  'ide-debugger-frame.h',
  'ide-debugger-instruction.h',
  'ide-debugger-library.h',
  'ide-debugger-register.h',
  'ide-debugger-thread-group.h',
  'ide-debugger-thread.h',
  'ide-debugger-types.h',
  'ide-debugger-variable.h',
]

debugger_sources = [
  'ide-debug-manager.c',
  'ide-debugger.c',
  'ide-debugger-breakpoint.c',
  'ide-debugger-breakpoints.c',
  'ide-debugger-frame.c',
  'ide-debugger-instruction.c',
  'ide-debugger-library.c',
  'ide-debugger-register.c',
  'ide-debugger-thread-group.c',
  'ide-debugger-thread.c',
  'ide-debugger-types.c',
  'ide-debugger-variable.c',
]

debugger_private_sources = [
  'ide-debugger-actions.c',
  'ide-debugger-address-map.c',
  'ide-debugger-address-map.h',
  'ide-debugger-breakpoints-view.c',
  'ide-debugger-breakpoints-view.h',
  'ide-debugger-controls.c',
  'ide-debugger-controls.h',
  'ide-debugger-disassembly-view.c',
  'ide-debugger-disassembly-view.h',
  'ide-debugger-editor-addin.c',
  'ide-debugger-editor-addin.h',
  'ide-debugger-fallbacks.c',
  'ide-debugger-gutter-renderer.c',
  'ide-debugger-gutter-renderer.h',
  'ide-debugger-libraries-view.c',
  'ide-debugger-libraries-view.h',
  'ide-debugger-locals-view.c',
  'ide-debugger-locals-view.h',
  'ide-debugger-plugin.c',
  'ide-debugger-private.h',
  'ide-debugger-registers-view.c',
  'ide-debugger-registers-view.h',
  'ide-debugger-threads-view.c',
  'ide-debugger-threads-view.h',
]

libide_public_headers += files(debugger_headers)
libide_public_sources += files(debugger_sources)
libide_private_sources += files(debugger_private_sources)

install_headers(debugger_headers, subdir: join_paths(libide_header_subdir, 'debugger'))
