plugindir = join_paths(get_option('libdir'), 'gnome-builder/plugins')
plugindatadir = join_paths(get_option('datadir'), 'gnome-builder/plugins')

gnome_builder_plugins_sources = ['gnome-builder-plugins.c']
gnome_builder_plugins_args = []
gnome_builder_plugins_deps = [libpeas_dep, libide_plugin_dep, libide_dep]
gnome_builder_plugins_link_with = []
gnome_builder_plugins_link_whole = []
gnome_builder_plugins_link_deps = join_paths(meson.current_source_dir(), 'plugins.map')
gnome_builder_plugins_link_args = [
  '-Wl,--version-script,' + gnome_builder_plugins_link_deps,
]

subdir('autotools')
subdir('autotools-templates')
subdir('beautifier')
subdir('c-pack')
subdir('cargo')
subdir('clang')
subdir('cmake')
subdir('code-index')
subdir('color-picker')
subdir('command-bar')
subdir('comment-code')
subdir('create-project')
subdir('ctags')
subdir('documentation-card')
subdir('devhelp')
subdir('eslint')
subdir('file-search')
subdir('find-other-file')
subdir('flatpak')
subdir('gcc')
subdir('gdb')
subdir('gettext')
subdir('git')
subdir('gjs-symbols')
subdir('gnome-code-assistance')
subdir('history')
subdir('html-completion')
subdir('html-preview')
subdir('jedi')
subdir('jhbuild')
subdir('make')
subdir('meson')
subdir('meson-templates')
subdir('mingw')
subdir('mono')
subdir('newcomers')
subdir('notification')
subdir('npm')
subdir('phpize')
subdir('project-tree')
subdir('python-gi-imports-completion')
subdir('python-pack')
subdir('quick-highlight')
subdir('retab')
subdir('rust-langserv')
subdir('rustup')
subdir('spellcheck')
subdir('support')
subdir('symbol-tree')
subdir('sysmon')
subdir('sysprof')
subdir('terminal')
subdir('todo')
subdir('vala-pack')
subdir('valgrind')
subdir('xml-pack')

gnome_builder_plugins = shared_library(
  'gnome-builder-plugins',
  gnome_builder_plugins_sources,

   dependencies: gnome_builder_plugins_deps,
   link_depends: 'plugins.map',
         c_args: gnome_builder_plugins_args,
      link_args: gnome_builder_plugins_link_args,
      link_with: gnome_builder_plugins_link_with,
        install: true,
    install_dir: pkglibdir,
  install_rpath: pkglibdir_abs,
)

gnome_builder_plugins_dep = declare_dependency(
   dependencies: libide_deps,
      link_with: gnome_builder_plugins_link_with + [gnome_builder_plugins],
     link_whole: gnome_builder_plugins_link_whole,
)

status += [
  'Plugins:',
  '',
  'Autotools ............. : @0@'.format(get_option('with_autotools')),
  'Beautifier ............ : @0@'.format(get_option('with_beautifier')),
  'C Language Pack ....... : @0@'.format(get_option('with_c_pack')),
  'Cargo ................. : @0@'.format(get_option('with_cargo')),
  'Clang ................. : @0@'.format(get_option('with_clang')),
  'CMake ................. : @0@'.format(get_option('with_cmake')),
  'Color Picker .......... : @0@'.format(get_option('with_color_picker')),
  'Command Bar ........... : @0@'.format(get_option('with_command_bar')),
  'Comment Code .......... : @0@'.format(get_option('with_comment_code')),
  'Project Wizard ........ : @0@'.format(get_option('with_create_project')),
  'CTags ................. : @0@'.format(get_option('with_ctags')),
  'Devhelp ............... : @0@'.format(get_option('with_devhelp')),
  'ESLint ................ : @0@'.format(get_option('with_eslint')),
  'File Search ........... : @0@'.format(get_option('with_file_search')),
  'Find other file ....... : @0@'.format(get_option('with_find_other_file')),
  'Flatpak ............... : @0@'.format(get_option('with_flatpak')),
  'GCC ................... : @0@'.format(get_option('with_gcc')),
  'GDB ................... : @0@'.format(get_option('with_gdb')),
  'Gettext ............... : @0@'.format(get_option('with_gettext')),
  'Git ................... : @0@'.format(get_option('with_git')),
  'GJS Symbol Resolver ... : @0@'.format(get_option('with_gjs_symbols')),
  'GNOME Code Assistance . : @0@'.format(get_option('with_gnome_code_assistance')),
  'History ............... : @0@'.format(get_option('with_history')),
  'HTML Completion ....... : @0@'.format(get_option('with_html_completion')),
  'HTML Preview .......... : @0@'.format(get_option('with_html_preview')),
  'Python Jedi ........... : @0@'.format(get_option('with_jedi')),
  'JHBuild ............... : @0@'.format(get_option('with_jhbuild')),
  'Make .................. : @0@'.format(get_option('with_make')),
  'Meson ................. : @0@'.format(get_option('with_meson')),
  'MinGW ................. : @0@'.format(get_option('with_mingw')),
  'Mono .................. : @0@'.format(get_option('with_mono')),
  'Notifications ......... : @0@'.format(get_option('with_notification')),
  'Node Package Manager .. : @0@'.format(get_option('with_npm')),
  'PHPize ................ : @0@'.format(get_option('with_phpize')),
  'Project Tree .......... : @0@'.format(get_option('with_project_tree')),
  'Python GI Completion .. : @0@'.format(get_option('with_python_gi_imports_completion')),
  'Python Language Pack .. : @0@'.format(get_option('with_python_pack')),
  'Quick Highlight ....... : @0@'.format(get_option('with_quick_highlight')),
  'Retab ................. : @0@'.format(get_option('with_retab')),
  'Rust Language Server .. : @0@'.format(get_option('with_rust_langserv')),
  'RustUp ................ : @0@'.format(get_option('with_rustup')),
  'Spellchecking ......... : @0@'.format(get_option('with_spellcheck')),
  'Support Tool .......... : @0@'.format(get_option('with_support')),
  'Symbol Tree ........... : @0@'.format(get_option('with_symbol_tree')),
  'System Monitor ........ : @0@'.format(get_option('with_sysmon')),
  'Sysprof Profiler ...... : @0@'.format(get_option('with_sysprof')),
  'Todo .................. : @0@'.format(get_option('with_todo')),
  'Vala Language Pack .... : @0@'.format(get_option('with_vala_pack')),
  'Valgrind .............. : @0@'.format(get_option('with_valgrind')),
  'XML Language Pack ..... : @0@'.format(get_option('with_xml_pack')),
  '', '',

  'Templates:',
  '',
  'Autotools ............. : @0@'.format(get_option('with_autotools_templates')),
  'Meson ................. : @0@'.format(get_option('with_meson_templates')),
  '', ''
]
