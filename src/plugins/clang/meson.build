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
  'ide-clang-completion-provider.c',
  'ide-clang-completion-provider.h',
  'ide-clang-diagnostic-provider.c',
  'ide-clang-diagnostic-provider.h',
  'ide-clang-highlighter.c',
  'ide-clang-highlighter.h',
  'ide-clang-preferences-addin.c',
  'ide-clang-preferences-addin.h',
  'ide-clang-symbol-node.c',
  'ide-clang-symbol-node.h',
  'ide-clang-symbol-resolver.c',
  'ide-clang-symbol-resolver.h',
  'ide-clang-symbol-tree.c',
  'ide-clang-symbol-tree.h',
  'ide-clang-client.c',
  'ide-clang-client.h',
  'clang-plugin.c',
]

gnome_builder_clang_sources = [
  'gnome-builder-clang.c',
  'ide-clang.c',
]

add_languages('cpp') # Needed for llvm dep
llvm_dep = dependency('llvm', version: '>= 3.5')
clang_include = llvm_dep.get_configtool_variable('includedir')
clang_libdir = llvm_dep.get_configtool_variable('libdir').split(' ')

if not cc.has_header('clang-c/Index.h', args: '-I' + clang_include)
  error('clang: Failed to find headers')
endif

clang_dep = declare_dependency(
         dependencies: cc.find_library('libclang', dirs: clang_libdir),
  include_directories: include_directories(clang_include),
)

clang_includes_dep = declare_dependency(
  include_directories: include_directories(clang_include),
)

gnome_builder_plugins_deps += [clang_includes_dep]
gnome_builder_plugins_sources += files(clang_sources)
gnome_builder_plugins_sources += clang_resources[0]

gnome_builder_clang_deps = [ clang_dep, libide_deps, libide_dep ]

executable('gnome-builder-clang', gnome_builder_clang_sources,
      dependencies: gnome_builder_clang_deps,
           gui_app: false,
           install: true,
       install_dir: get_option('libexecdir'),
            c_args: exe_c_args,
         link_args: exe_link_args,
     install_rpath: pkglibdir_abs,
)

executable('test-daemon', ['test-daemon.c'],
      dependencies: [libjsonrpc_glib_dep],
           gui_app: false,
           install: false,
            c_args: exe_c_args,
         link_args: exe_link_args,
)

endif
