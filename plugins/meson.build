plugindir = join_paths(get_option('libdir'), 'gnome-builder/plugins')
plugindatadir = join_paths(get_option('datadir'), 'gnome-builder/plugins')
plugin_deps = [
  libpeas_dep,
  libide_plugin_dep,
]

plugin_link_deps = join_paths(meson.current_source_dir(), 'plugins.map')
plugin_link_args = [
  '-Wl,--version-script,' + plugin_link_deps,
]

subdir('autotools')
subdir('autotools-templates')
subdir('beautifier')
subdir('c-pack')
subdir('cargo')
subdir('clang')
subdir('cmake')
subdir('color-picker')
subdir('command-bar')
subdir('comment-code')
subdir('create-project')
subdir('ctags')
subdir('devhelp')
subdir('eslint')
subdir('file-search')
subdir('find-other-file')
subdir('flatpak')
subdir('fpaste')
subdir('gcc')
subdir('gettext')
subdir('git')
subdir('gnome-code-assistance')
subdir('html-completion')
subdir('html-preview')
subdir('jedi')
subdir('jhbuild')
subdir('make')
subdir('meson')
subdir('meson-templates')
subdir('mingw')
subdir('mono')
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
subdir('support')
subdir('symbol-tree')
subdir('sysmon')
subdir('sysprof')
subdir('terminal')
subdir('todo')
subdir('vala-pack')
subdir('valgrind')
subdir('xml-pack')

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
  'Fpaste ................ : @0@'.format(get_option('with_fpaste')),
  'GCC ................... : @0@'.format(get_option('with_gcc')),
  'Gettext ............... : @0@'.format(get_option('with_gettext')),
  'Git ................... : @0@'.format(get_option('with_git')),
  'GNOME Code Assistance . : @0@'.format(get_option('with_gnome_code_assistance')),
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
  'Support Tool .......... : @0@'.format(get_option('with_support')),
  'Symbol Tree ........... : @0@'.format(get_option('with_symbol_tree')),
  'System Monitor ........ : @0@'.format(get_option('with_sysmon')),
  'Sysprof Profiler ...... : @0@'.format(get_option('with_sysprof')),
  'Terminal .............. : @0@'.format(get_option('with_terminal')),
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
