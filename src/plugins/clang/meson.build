if get_option('plugin_clang')

plugins_sources += files([
  'clang-plugin.c',
  'ide-clang-client.c',
  'ide-clang-code-index-entries.c',
  'ide-clang-code-indexer.c',
  'ide-clang-completion-item.c',
  'ide-clang-completion-provider.c',
  'ide-clang-diagnostic-provider.c',
  'ide-clang-highlighter.c',
  'ide-clang-proposals.c',
  'ide-clang-rename-provider.c',
  'ide-clang-symbol-node.c',
  'ide-clang-symbol-resolver.c',
  'ide-clang-symbol-tree.c',
  'gbp-clang-tweaks-addin.c',
])

gnome_builder_clang_sources = [
  'gnome-builder-clang.c',
  'ide-clang.c',
]

plugin_clang_resources = gnome.compile_resources(
  'clang-resources',
  'clang.gresource.xml',
  c_name: 'gbp_clang',
)

plugins_sources += plugin_clang_resources

add_languages('cpp', native: false) # Needed for llvm dep
llvm_dep = dependency('llvm', version: '>= 3.5', method: 'config-tool')
clang_include = llvm_dep.get_variable('includedir')
clang_libdir = llvm_dep.get_variable('libdir').split(' ')

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

plugins_deps += [clang_includes_dep]

gnome_builder_clang_deps = [
  clang_dep,
  libjsonrpc_glib_dep,

  libide_code_dep,
]

executable('gnome-builder-clang', gnome_builder_clang_sources,
      dependencies: gnome_builder_clang_deps,
           install: true,
       install_dir: get_option('libexecdir'),
            c_args: exe_c_args,
               pie: true,
     install_rpath: pkglibdir_abs,
)

executable('test-daemon', ['test-daemon.c'],
      dependencies: [libjsonrpc_glib_dep],
           install: false,
            c_args: exe_c_args,
               pie: true,
)

install_data(['org.gnome.builder.clang.gschema.xml'], install_dir: schema_dir)

endif
