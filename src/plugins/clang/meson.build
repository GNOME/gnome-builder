if get_option('with_clang')

clang_resources = gnome.compile_resources(    
  'ide-clang-resources',                      
  'clang.gresource.xml',                      
  c_name: 'ide_clang',                        
)                                           

clang_sources = [
  'ide-clang-code-index-entries.c',
  'ide-clang-code-index-entries.h',
  'ide-clang-code-indexer.c',
  'ide-clang-code-indexer.h',
  'ide-clang-completion-item.c',
  'ide-clang-completion-item.h',
  'ide-clang-completion-item-private.h',
  'ide-clang-completion-provider.c',
  'ide-clang-completion-provider.h',
  'ide-clang-diagnostic-provider.c',
  'ide-clang-diagnostic-provider.h',
  'ide-clang-highlighter.c',
  'ide-clang-highlighter.h',
  'ide-clang-preferences-addin.c',
  'ide-clang-preferences-addin.h',
  'ide-clang-private.h',
  'ide-clang-service.c',
  'ide-clang-service.h',
  'ide-clang-symbol-node.c',
  'ide-clang-symbol-node.h',
  'ide-clang-symbol-resolver.c',
  'ide-clang-symbol-resolver.h',
  'ide-clang-symbol-tree.c',
  'ide-clang-symbol-tree.h',
  'ide-clang-translation-unit.c',
  'ide-clang-translation-unit.h',
  'clang-plugin.c',
]

llvm_config = find_program(
  'llvm-config',
  'llvm-config-5.0', 'llvm-config50',
  'llvm-config-4.0', 'llvm-config40',
  'llvm-config-3.9', 'llvm-config39',
  'llvm-config-3.8', 'llvm-config38',
  'llvm-config-3.7', 'llvm-config37',
  'llvm-config-3.6', 'llvm-config36',
  'llvm-config-3.5', 'llvm-config35',
  'llvm-config-6.0', 'llvm-config-devel',
)

ret = run_command(llvm_config, '--includedir')
if ret.returncode() != 0
  error('clang: Failed to run ' + llvm_config.path())
endif
clang_include = ret.stdout().strip()

ret = run_command(llvm_config, '--libdir')
if ret.returncode() != 0
  error('clang: Failed to run ' + llvm_config.path())
endif
clang_libdir = ret.stdout().strip().split(' ')

if not cc.has_header('clang-c/Index.h', args: '-I' + clang_include)
  error('clang: Failed to find headers')
endif

clang_dep = declare_dependency(
         dependencies: cc.find_library('libclang', dirs: clang_libdir),
  include_directories: include_directories(clang_include),
)

gnome_builder_plugins_deps += [clang_dep]
gnome_builder_plugins_sources += files(clang_sources)
gnome_builder_plugins_sources += clang_resources[0]

endif
