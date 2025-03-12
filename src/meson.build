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
gnome_builder_gir_extra_args = []
gnome_builder_main_headers = []

# To allow all resources to be initialized with static constructors
# inside the final executable, we delay compiling them until the
# final binary (otherwise they are silenty dropped when linking).

exe_link_args = []
exe_c_args = []

# Setup variables for some libide tests early
test_gi_typelib_path = ['@0@/src'.format(meson.project_build_root())]
test_typelib_deps = [
  libadwaita_dep,
  libgio_dep,
  libgir_dep,
  libgtk_dep,
  libgtksource_dep,
  libjson_glib_dep,
  libjsonrpc_glib_dep,
  libpanel_dep,
  libpeas_dep,
  libtemplate_glib_dep,
  libvte_dep,
]
foreach test_typelib_dep: test_typelib_deps
  test_gi_typelib_path += [join_paths(test_typelib_dep.get_variable('libdir'), 'girepository-1.0')]
endforeach
test_env = [
  'GI_TYPELIB_PATH=@0@'.format(':'.join(test_gi_typelib_path)),
  'G_TEST_SRCDIR=@0@/tests'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@/tests'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@/data/gsettings'.format(meson.project_build_root()),
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
  'NO_AT_BRIDGE=1',
]
test_cflags = [
  '-DTEST_DATA_DIR="@0@/tests/data/"'.format(meson.current_source_dir()),
  '-I' + join_paths(meson.project_source_root(), 'src'),
]

gnome_builder_pkg_requires = [
  'gio-2.0',
  'gio-unix-2.0',
  'gtk4',
  'vte-2.91-gtk4',
  'gtksourceview-5',
  'template-glib-1.0',
  'libpeas-2',
]

gnome_builder_gir_includes = [
  'Adw-1',
  'Dex-1',
  'Gio-2.0',
  'Gtk-4.0',
  'GtkSource-5',
  'Panel-1',
  'Peas-2',
  'Template-1.0',
  'Vte-3.91',
]

subdir('libide')
subdir('plugins')

gnome_builder_deps = [
  libgio_dep,
  libgiounix_dep,
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
  libide_threading_dep,
  libide_tweaks_dep,
  libide_vcs_dep,
  libide_tree_dep,
]

gnome_builder_static = [
  libide_code,
  libide_core,
  libide_debugger,
  libide_editor,
  libide_foundry,
  libide_greeter,
  libide_gui,
  libide_io,
  libide_lsp,
  libide_plugins,
  libide_projects,
  libide_search,
  libide_sourceview,
  libide_terminal,
  libide_threading,
  libide_tree,
  libide_vcs,

  plugins,
]

if get_option('webkit').enabled()
  gnome_builder_deps += [libide_webkit_dep]
  gnome_builder_static += [libide_webkit]
endif

if libsysprof_capture.found()
  gnome_builder_deps += libsysprof_capture
endif

gnome_builder = executable('gnome-builder', 'main.c', 'bug-buddy.c',
           install: true,
            c_args: libide_args + exe_c_args + release_args,
         link_args: exe_link_args,
               pie: true,
        link_whole: gnome_builder_static,
     install_rpath: pkglibdir_abs,
      dependencies: gnome_builder_deps,
    export_dynamic: true,
)

# We use requires: instead of libraries: so that our link args of
# things like -Wl,--require-defined= do not leak into the .pc file.
pkgconfig.generate(
      subdirs: gnome_builder_include_subdirs,
      version: meson.project_version(),
         name: 'gnome-builder-@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION),
     filebase: 'gnome-builder-@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION),
  description: 'Contains the plugin container for Builder.',
  install_dir: join_paths(get_option('libdir'), 'pkgconfig'),
     requires: gnome_builder_pkg_requires,
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
             includes: gnome_builder_gir_includes,
               header: gnome_builder_main_headers,
              install: true,
      install_dir_gir: pkggirdir,
  install_dir_typelib: pkgtypelibdir,
           extra_args: gnome_builder_gir_extra_args,
)

# tests last so they can reference libide_gir as test(depends:)
subdir('tests')
