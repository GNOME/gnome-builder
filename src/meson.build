# These are updated by subdir() meson.build files so that we
# can generate gir files and what not appropriate for the
# final binary (which statically links their .a libraries)
gnome_builder_include_subdirs = []
gnome_builder_public_sources = []
gnome_builder_public_headers = []
gnome_builder_private_sources = []
gnome_builder_private_headers = []
gnome_builder_generated_sources = []
gnome_builder_generated_headers = []
gnome_builder_gir_extra_args = ['--pkg-export=gnome-builder-1.0']
# To allow all resources to be initialized with static constructors
# inside the final executable, we delay compiling them until the
# final binary (otherwise they are silenty dropped when linking).

exe_link_args = [ '-export-dynamic' ]
exe_c_args = []

subdir('gstyle')
subdir('libide')
subdir('tests')
subdir('plugins')

gnome_builder_deps = [
  libgio_dep,
  libgiounix_dep,
  libdazzle_dep,
  libgtk_dep,

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
  libide_vcs_dep,
  libide_webkit_dep,
  libide_tree_dep,
]

if get_option('fusermount_wrapper')

  executable('fusermount', ['fusermount-wrapper.c'],
             gui_app: false,
             install: true,
         install_dir: get_option('bindir'),
              c_args: exe_c_args + release_args,
                 pie: true,
       install_rpath: pkglibdir_abs,
        dependencies: [libglib_dep],
  )

  install_data(['rofiles-fuse'],
         install_dir: get_option('bindir'),
        install_mode: 'rwxr-xr-x',
  )

endif

gnome_builder = executable('gnome-builder', 'main.c', 'bug-buddy.c',
           gui_app: true,
           install: true,
            c_args: libide_args + exe_c_args + release_args,
         link_args: exe_link_args,
               pie: true,
        link_whole: plugins,
     install_rpath: pkglibdir_abs,
      dependencies: gnome_builder_deps,
)

# We use requires: instead of libraries: so that our link args of
# things like -Wl,--require-defined= do not leak into the .pc file.
pkgconfig.generate(
      subdirs: gnome_builder_include_subdirs,
      version: meson.project_version(),
         name: 'gnome-builder-@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION),
     filebase: 'gnome-builder-@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION),
  description: 'Contains the plugin container for Builder.',
  install_dir: join_paths(pkglibdir, 'pkgconfig'),
     requires: [ 'gio-2.0', 'gio-unix-2.0', 'gtk+-3.0', 'libdazzle-1.0' ],
)

libide_gir = gnome.generate_gir(gnome_builder,
              sources: gnome_builder_generated_headers +
                       gnome_builder_generated_sources +
                       gnome_builder_public_headers +
                       gnome_builder_public_sources,
            nsversion: libide_api_version,
            namespace: 'Ide',
        symbol_prefix: 'ide',
    identifier_prefix: 'Ide',
             includes: [ 'Gio-2.0', 'Gtk-3.0', 'Dazzle-1.0', 'Peas-1.0', 'Vte-2.91', 'GtkSource-4', 'Template-1.0' ],
              install: true,
      install_dir_gir: pkggirdir,
  install_dir_typelib: pkgtypelibdir,
           extra_args: gnome_builder_gir_extra_args,
)

configure_file(
          input: 'libide.deps',
         output: 'libide-' + libide_api_version + '.deps',
           copy: true,
        install: true,
    install_dir: pkgvapidir,
)

configure_file(
          input: 'Ide.metadata.in',
         output: 'Ide-@0@.@1@.metadata'.format(MAJOR_VERSION, MINOR_VERSION),
           copy: true,
)

libide_vapi = gnome.generate_vapi('libide-' + libide_api_version,
        sources: libide_gir[0],
        install: true,
    install_dir: pkgvapidir,
  metadata_dirs: [meson.current_build_dir()],
       packages: [ 'gio-2.0',
                   'gtk+-3.0',
                   'gtksourceview-4',
                   'json-glib-1.0',
                   'libdazzle-1.0',
                   'libpeas-1.0',
                   'template-glib-1.0',
                   'vte-2.91' ],
)

# Must be after vapi generation
subdir('plugins/vala-pack')
