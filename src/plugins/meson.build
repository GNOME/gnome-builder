plugindir = join_paths(get_option('libdir'), 'gnome-builder/plugins')
plugindatadir = join_paths(get_option('datadir'), 'gnome-builder/plugins')

plugins_sources = []
plugins_include_directories = []
plugins_generated_sources = []
plugins_link_with = []

plugins_deps = [
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
  libide_threading_dep,
  libide_tree_dep,
  libide_vcs_dep,
]

if get_option('webkit').enabled()
  plugins_deps += [libide_webkit_dep]
endif

subdir('arduino')
subdir('astro-language-server')
subdir('auto-save')
subdir('autotools')
subdir('bash-language-server')
subdir('blueprint')
subdir('buildconfig')
subdir('buildstream')
subdir('buildsystem')
subdir('buildui')
subdir('buffer-monitor')
subdir('cargo')
subdir('clang')
subdir('clangd')
subdir('clang-format')
subdir('cmake')
subdir('codespell')
subdir('code-index')
# subdir('codesearch')
subdir('codeshot')
subdir('codeui')
subdir('css-preview')
subdir('comment-code')
subdir('copyright')
subdir('c-pack')
subdir('create-project')
subdir('ctags')
subdir('debuggerui')
subdir('deviced')
subdir('doap')
subdir('dspy')
subdir('dub')
subdir('editorui')
subdir('editorconfig')
subdir('elixir-ls')
subdir('eslint')
subdir('file-search')
subdir('find-other-file')
subdir('flake8')
subdir('flatpak')
subdir('gcc')
subdir('gdb')
subdir('gdiagnose')
subdir('gettext')
subdir('git')
subdir('glsl-language-server')
subdir('golang')
subdir('gopls')
subdir('gradle')
subdir('greeter')
subdir('grep')
subdir('hadolint')
subdir('host')
subdir('html-completion')
subdir('html-preview')
subdir('intelephense')
subdir('jdtls')
subdir('jedi-language-server')
subdir('jhbuild')
subdir('ls')
subdir('lua-language-server')
subdir('make')
subdir('make-templates')
subdir('manuals')
subdir('markdown-preview')
subdir('markdown-indenter')
subdir('maven')
subdir('menu-search')
subdir('meson')
subdir('mesonlsp')
subdir('meson-templates')
subdir('messages')
subdir('modelines')
subdir('mono')
subdir('newcomers')
subdir('notification')
subdir('npm')
subdir('omni-gutter')
subdir('open-with-external')
subdir('platformui')
subdir('phpize')
subdir('podman')
subdir('project-tree')
subdir('projectui')
subdir('pygi')
subdir('python-lsp-server')
subdir('qemu')
subdir('quick-highlight')
subdir('recent')
subdir('restore-cursor')
subdir('retab')
subdir('rstcheck')
subdir('rubocop')
subdir('ruff')
subdir('rust-analyzer')
subdir('sdkui')
subdir('serve-d')
subdir('sessionui')
subdir('shellcheck')
subdir('shellcmd')
subdir('shortcutui')
subdir('snippets')
subdir('sourcekit-lsp')
subdir('spellcheck')
subdir('sphinx-preview')
subdir('stylelint')
subdir('swift')
subdir('swiftlint')
subdir('swift-format')
subdir('symbol-tree')
subdir('sysprof')
subdir('terminal')
subdir('testui')
subdir('todo')
subdir('top-match')
subdir('ts-language-server')
subdir('trim-spaces')
subdir('update-dependencies')
subdir('update-manager')
subdir('valac')
subdir('vala-indenter')
subdir('vala-language-server')
subdir('valgrind')
subdir('vcsui')
subdir('vhdl-language-server')
subdir('vim')
subdir('waf')
subdir('web-browser')
subdir('words')
subdir('xml-pack')
subdir('zls')

plugins = static_library('plugins', plugins_sources,
           dependencies: plugins_deps,
                 c_args: release_args,
    include_directories: plugins_include_directories,
              link_with: plugins_link_with,
  gnu_symbol_visibility: 'hidden',
)

status += [
  'Plugins:',
  '',
  'Arduino .............................. : @0@'.format(get_option('plugin_arduino')),
  'Autotools ............................ : @0@'.format(get_option('plugin_autotools')),
  'BuildStream .......................... : @0@'.format(get_option('plugin_buildstream')),
  'C Pack ............................... : @0@'.format(get_option('plugin_c_pack')),
  'Cargo ................................ : @0@'.format(get_option('plugin_cargo')),
  'Clang Format ......................... : @0@'.format(get_option('plugin_clang_format')),
  'CMake ................................ : @0@'.format(get_option('plugin_cmake')),
  'Codeshot ............................. : @0@'.format(get_option('plugin_codeshot')),
  'CSS Preview .......................... : @0@'.format(get_option('plugin_css_preview')),
  'Codespell ............................ : @0@'.format(get_option('plugin_codespell')),
  'Code Index ........................... : @0@'.format(get_option('plugin_code_index')),
  'Code Search .......................... : @0@'.format(get_option('plugin_codesearch')),
  'CTags ................................ : @0@'.format(get_option('plugin_ctags')),
  'Copyright ............................ : @0@'.format(get_option('plugin_copyright')),
  'Deviced .............................. : @0@'.format(get_option('plugin_deviced')),
  'D-Bus Spy ............................ : @0@'.format(get_option('plugin_dspy')),
  'Dub .................................. : @0@'.format(get_option('plugin_dub')),
  'Editorconfig ......................... : @0@'.format(get_option('plugin_editorconfig')),
  'ESLint ............................... : @0@'.format(get_option('plugin_eslint')),
  'File Search .......................... : @0@'.format(get_option('plugin_file_search')),
  'Flake8 ............................... : @0@'.format(get_option('plugin_flake8')),
  'Flatpak .............................. : @0@'.format(get_option('plugin_flatpak')),
  'GDB .................................. : @0@'.format(get_option('plugin_gdb')),
  'Gettext .............................. : @0@'.format(get_option('plugin_gettext')),
  'Git .................................. : @0@'.format(get_option('plugin_git')),
  'Gradle ............................... : @0@'.format(get_option('plugin_gradle')),
  'Grep ................................. : @0@'.format(get_option('plugin_grep')),
  'Hadolint ............................. : @0@'.format(get_option('plugin_hadolint')),
  'HTML Completion ...................... : @0@'.format(get_option('plugin_html_completion')),
  'HTML Preview ......................... : @0@'.format(get_option('plugin_html_preview')),
  'JHBuild .............................. : @0@'.format(get_option('plugin_jhbuild')),
  'Make ................................. : @0@'.format(get_option('plugin_make')),
  'Make Templates ....................... : @0@'.format(get_option('plugin_make_templates')),
  'Markdown Preview ..................... : @0@'.format(get_option('plugin_markdown_preview')),
  'Markdown Indenter .................... : @0@'.format(get_option('plugin_markdown_indenter')),
  'Maven ................................ : @0@'.format(get_option('plugin_maven')),
  'Meson ................................ : @0@'.format(get_option('plugin_meson')),
  'Meson Templates ...................... : @0@'.format(get_option('plugin_meson_templates')),
  'Modelines ............................ : @0@'.format(get_option('plugin_modelines')),
  'Mono ................................. : @0@'.format(get_option('plugin_mono')),
  'Newcomers ............................ : @0@'.format(get_option('plugin_newcomers')),
  'Notifications ........................ : @0@'.format(get_option('plugin_notification')),
  'Npm .................................. : @0@'.format(get_option('plugin_npm')),
  'PHPize ............................... : @0@'.format(get_option('plugin_phpize')),
  'Podman ............................... : @0@'.format(get_option('plugin_podman')),
  'PyGObject Imports .................... : @0@'.format(get_option('plugin_pygi')),
  'Qemu ................................. : @0@'.format(get_option('plugin_qemu')),
  'Quick Highlight ...................... : @0@'.format(get_option('plugin_quick_highlight')),
  'Retab ................................ : @0@'.format(get_option('plugin_retab')),
  'rstcheck ............................. : @0@'.format(get_option('plugin_rstcheck')),
  'Sphinx Preview (reStructuredText) .... : @0@'.format(get_option('plugin_sphinx_preview')),
  'Rubocop .............................. : @0@'.format(get_option('plugin_rubocop')),
  'Spellcheck ........................... : @0@'.format(get_option('plugin_spellcheck')),
  'Swift ................................ : @0@'.format(get_option('plugin_swift')),
  'Swift-Format ......................... : @0@'.format(get_option('plugin_swiftformat')),
  'SwiftLint ............................ : @0@'.format(get_option('plugin_swiftlint')),
  'Stylelint ............................ : @0@'.format(get_option('plugin_stylelint')),
  'Sysprof .............................. : @0@'.format(get_option('plugin_sysprof')),
  'Todo ................................. : @0@'.format(get_option('plugin_todo')),
  'Update Manager ....................... : @0@'.format(get_option('plugin_update_manager')),
  'Vala Compiler ........................ : @0@'.format(get_option('plugin_valac')),
  'Vala Indenter ........................ : @0@'.format(get_option('plugin_vala_indenter')),
  'Valgrind ............................. : @0@'.format(get_option('plugin_valgrind')),
  'Waf .................................. : @0@'.format(get_option('plugin_waf')),
  'Word Completion ...................... : @0@'.format(get_option('plugin_words')),
  'XML Pack ............................. : @0@'.format(get_option('plugin_xml_pack')),
  '',
  'Language Servers:',
  '',
  'astro-language-server ........ (Astro) : @0@'.format(get_option('plugin_astro_language_server')),
  'bash-language-server .......... (Bash) : @0@'.format(get_option('plugin_bash_language_server')),
  'blueprint ................ (Blueprint) : @0@'.format(get_option('plugin_blueprint')),
  'clangd ............... (C, C++, Obj-C) : @0@ **'.format(get_option('plugin_clangd')),
  'elixir-ls ................... (Elixir) : @0@'.format(get_option('plugin_elixir_ls')),
  'glsl-language-server .......... (GLSL) : @0@'.format(get_option('plugin_glsl_language_server')),
  'gnome-builder-clang .. (C, C++, Obj-C) : @0@'.format(get_option('plugin_clang')),
  'gopls ........................... (Go) : @0@'.format(get_option('plugin_gopls')),
  'intelephense ................... (PHP) : @0@'.format(get_option('plugin_intelephense')),
  'jdtls ......................... (Java) : @0@'.format(get_option('plugin_jdtls')),
  'jedi-language-server ........ (Python) : @0@ **'.format(get_option('plugin_jedi_language_server')),
  'lua-language-server ............ (Lua) : @0@'.format(get_option('plugin_lua_language_server')),
  'mesonlsp ..................... (Meson) : @0@'.format(get_option('plugin_mesonlsp')),
  'python-lsp-server ........... (Python) : @0@'.format(get_option('plugin_python_lsp_server')),
  'serve-d .......................... (D) : @0@'.format(get_option('plugin_serve_d')),
  'SourceKit .................... (Swift) : @0@'.format(get_option('plugin_sourcekit_lsp')),
  'ruff ........................ (Python) : @0@'.format(get_option('plugin_ruff')),
  'rust-analyzer ................. (Rust) : @0@'.format(get_option('plugin_rust_analyzer')),
  'ts-language-server ... (JS/TypeScript) : @0@'.format(get_option('plugin_ts_language_server')),
  'vala-language-server .......... (Vala) : @0@'.format(get_option('plugin_vala_language_server')),
  'vhdl-language-server .......... (VHDL) : @0@'.format(get_option('plugin_vhdl_language_server')),
  'zls ........................ (Ziglang) : @0@'.format(get_option('plugin_zls')),
  '',
  ' ** Denotes plugins not recommended for distribution',
  '',
]
