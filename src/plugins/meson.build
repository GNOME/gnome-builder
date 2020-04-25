plugindir = join_paths(get_option('libdir'), 'gnome-builder/plugins')
plugindatadir = join_paths(get_option('datadir'), 'gnome-builder/plugins')

plugins_sources = []
plugins_include_directories = []
plugins_generated_sources = []
plugins_link_with = []

plugins_deps = [
  libdazzle_dep,
  libgtk_dep,
  libgtksource_dep,
  libjsonrpc_glib_dep,

  libide_code_dep,
  libide_core_dep,
  libide_debugger_dep,
  libide_editor_dep,
  libide_foundry_dep,
  libide_greeter_dep,
  libide_gui_dep,
  libide_io_dep,
  libide_lsp_dep,
  libide_plugins_dep,
  libide_projects_dep,
  libide_search_dep,
  libide_sourceview_dep,
  libide_terminal_dep,
  libide_themes_dep,
  libide_threading_dep,
  libide_tree_dep,
  libide_vcs_dep,
  libide_webkit_dep,
]

subdir('auto-save')
subdir('autotools')
subdir('beautifier')
subdir('buildconfig')
subdir('buildsystem')
subdir('buildui')
subdir('buffer-monitor')
subdir('cargo')
subdir('clang')
subdir('cmake')
subdir('code-index')
subdir('codeui')
subdir('color-picker')
subdir('command-bar')
subdir('comment-code')
subdir('copyright')
subdir('c-pack')
subdir('create-project')
subdir('ctags')
subdir('debuggerui')
subdir('devhelp')
subdir('deviceui')
subdir('deviced')
subdir('doap')
subdir('dspy')
subdir('editor')
subdir('editorconfig')
subdir('emacs')
subdir('eslint')
subdir('flatpak')
subdir('file-search')
subdir('find-other-file')
subdir('gcc')
subdir('gdb')
subdir('gettext')
subdir('git')
subdir('glade')
subdir('gnome-code-assistance')
subdir('go-langserv')
subdir('gjs-symbols')
subdir('gradle')
subdir('greeter')
subdir('grep')
subdir('gvls')
subdir('history')
subdir('html-completion')
subdir('html-preview')
subdir('jedi')
subdir('jhbuild')
subdir('line-spacing')
subdir('ls')
subdir('make')
subdir('maven')
subdir('meson')
subdir('meson-templates')
subdir('messages')
subdir('modelines')
subdir('mono')
subdir('newcomers')
subdir('notification')
subdir('npm')
subdir('omni-gutter')
subdir('phpize')
subdir('podman')
subdir('project-tree')
subdir('python-gi-imports-completion')
subdir('python-pack')
subdir('qemu')
subdir('quick-highlight')
subdir('recent')
subdir('restore-cursor')
subdir('retab')
subdir('rls')
subdir('rust-analyzer')
subdir('rustup')
subdir('shellcmd')
subdir('snippets')
subdir('spellcheck')
subdir('stylelint')
subdir('sublime')
subdir('support')
subdir('symbol-tree')
subdir('sysprof')
subdir('sysroot')
subdir('terminal')
subdir('testui')
subdir('todo')
subdir('trim-spaces')
subdir('update-manager')
subdir('vagrant')
subdir('vala-pack')
subdir('valgrind')
subdir('vcsui')
subdir('vim')
subdir('waf')
subdir('words')
subdir('xml-pack')

plugins = static_library('plugins', plugins_sources,
         dependencies: plugins_deps,
               c_args: release_args,
  include_directories: plugins_include_directories,
            link_with: plugins_link_with,
)

status += [
  'Plugins:',
  '',
  'Autotools ............. : @0@'.format(get_option('plugin_autotools')),
  'Beautifier ............ : @0@'.format(get_option('plugin_beautifier')),
  'C Pack ................ : @0@'.format(get_option('plugin_c_pack')),
  'Cargo ................. : @0@'.format(get_option('plugin_cargo')),
  'Clang ................. : @0@'.format(get_option('plugin_clang')),
  'CMake ................. : @0@'.format(get_option('plugin_cmake')),
  'Code Index ............ : @0@'.format(get_option('plugin_code_index')),
  'Color Pickr ........... : @0@'.format(get_option('plugin_color_picker')),
  'CTags ................. : @0@'.format(get_option('plugin_ctags')),
  'Copyright ............. : @0@'.format(get_option('plugin_copyright')),
  'Devhelp ............... : @0@'.format(get_option('plugin_devhelp')),
  'Deviced ............... : @0@'.format(get_option('plugin_deviced')),
  'D-Bus Spy ............. : @0@'.format(get_option('plugin_dspy')),
  'Editorconfig .......... : @0@'.format(get_option('plugin_editorconfig')),
  'ESLint ................ : @0@'.format(get_option('plugin_eslint')),
  'File Search ........... : @0@'.format(get_option('plugin_file_search')),
  'Flatpak ............... : @0@'.format(get_option('plugin_flatpak')),
  'GDB ................... : @0@'.format(get_option('plugin_gdb')),
  'Gettext ............... : @0@'.format(get_option('plugin_gettext')),
  'Git ................... : @0@'.format(get_option('plugin_git')),
  'GJS Symbols ........... : @0@'.format(get_option('plugin_gjs_symbols')),
  'Glade ................. : @0@'.format(get_option('plugin_glade')),
  'GNOME Code Assistance . : @0@'.format(get_option('plugin_gnome_code_assistance')),
  'Go Language Server .... : @0@'.format(get_option('plugin_go_langserv')),
  'Gradle ................ : @0@'.format(get_option('plugin_gradle')),
  'Grep .................. : @0@'.format(get_option('plugin_grep')),
  'HTML Completion ....... : @0@'.format(get_option('plugin_html_completion')),
  'HTML Preview .......... : @0@'.format(get_option('plugin_html_preview')),
  'Jedi .................. : @0@'.format(get_option('plugin_jedi')),
  'JHBuild ............... : @0@'.format(get_option('plugin_jhbuild')),
  'Make .................. : @0@'.format(get_option('plugin_make')),
  'Maven ................. : @0@'.format(get_option('plugin_maven')),
  'Meson ................. : @0@'.format(get_option('plugin_meson')),
  'Modelines ............. : @0@'.format(get_option('plugin_modelines')),
  'Mono .................. : @0@'.format(get_option('plugin_mono')),
  'Newcomers ............. : @0@'.format(get_option('plugin_newcomers')),
  'Notifications ......... : @0@'.format(get_option('plugin_notification')),
  'Npm ................... : @0@'.format(get_option('plugin_npm')),
  'PHPize ................ : @0@'.format(get_option('plugin_phpize')),
  'Podman ................ : @0@'.format(get_option('plugin_podman')),
  'Python Pack ........... : @0@'.format(get_option('plugin_python_pack')),
  'Qemu .................. : @0@'.format(get_option('plugin_qemu')),
  'Quick Highlight ....... : @0@'.format(get_option('plugin_quick_highlight')),
  'Retab ................. : @0@'.format(get_option('plugin_retab')),
  'RLS ................... : @0@'.format(get_option('plugin_rls')),
  'Rust Analyzer ......... : @0@'.format(get_option('plugin_rust_analyzer')),
  'Rustup ................ : @0@'.format(get_option('plugin_rustup')),
  'Spellcheck ............ : @0@'.format(get_option('plugin_spellcheck')),
  'Stylelint ............. : @0@'.format(get_option('plugin_stylelint')),
  'Sysprof ............... : @0@'.format(get_option('plugin_sysprof')),
  'Sysroot ............... : @0@'.format(get_option('plugin_sysroot')),
  'Todo .................. : @0@'.format(get_option('plugin_todo')),
  'Update Manager ........ : @0@'.format(get_option('plugin_update_manager')),
  'Vala Language Server... : @0@'.format(get_option('plugin_gvls')),
  'Vala Pack ............. : @0@'.format(get_option('plugin_vala')),
  'Vagrant ............... : @0@'.format(get_option('plugin_vagrant')),
  'Valgrind .............. : @0@'.format(get_option('plugin_valgrind')),
  'Waf ................... : @0@'.format(get_option('plugin_waf')),
  'Word Completion ....... : @0@'.format(get_option('plugin_words')),
  'XML Pack .............. : @0@'.format(get_option('plugin_xml_pack')),
  '',
]
