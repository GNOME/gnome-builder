if get_option('docs')

subdir('xml')

private_headers = ['config.h']
foreach source : gnome_builder_private_sources + gnome_builder_private_headers
  private_headers += ['@0@/@1@'.format(meson.source_root(), source)]
endforeach

# Locate the dependency documentation location to fix cross-references
libdazzle_prefix = dependency('libdazzle-1.0').get_pkgconfig_variable('prefix')
libdazzle_docpath = join_paths(libdazzle_prefix, 'share', 'gtk-doc', 'html')
glib_prefix = dependency('glib-2.0').get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')
json_prefix = dependency('json-glib-1.0').get_pkgconfig_variable('prefix')
json_docpath = join_paths(json_prefix, 'share', 'gtk-doc', 'html')
cairo_prefix = dependency('cairo').get_pkgconfig_variable('prefix')
cairo_docpath = join_paths(cairo_prefix, 'share', 'gtk-doc', 'html')
gi_prefix = dependency('gobject-introspection-1.0').get_pkgconfig_variable('prefix')
gi_docpath = join_paths(gi_prefix, 'share', 'gtk-doc', 'html')
gtk_prefix = dependency('gtk+-3.0').get_pkgconfig_variable('prefix')
gtk_docpath = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html')
gsv_prefix = dependency('gtksourceview-4').get_pkgconfig_variable('prefix')
gsv_docpath = join_paths(gsv_prefix, 'share', 'gtk-doc', 'html')
vte_prefix = dependency('vte-2.91').get_pkgconfig_variable('prefix')
vte_docpath = join_paths(vte_prefix, 'share', 'vte-doc', 'html')

# Locate our directory for documentation
docpath = join_paths(get_option('datadir'), 'gtk-doc', 'html')

libide_gtk_doc = shared_library('ide-gtk-doc',
            c_args: libide_args + release_args,
        link_whole: gnome_builder_static,
)

libide_gtk_doc_dep = declare_dependency(
         dependencies: gnome_builder_deps,
            link_with: libide_gtk_doc,
)

gnome.gtkdoc('libide',
           main_xml: 'libide-docs.sgml',
            src_dir: [
              join_paths(meson.source_root(), 'src', 'libide', 'core'),
              join_paths(meson.build_root(), 'src', 'libide', 'core'),

              join_paths(meson.source_root(), 'src', 'libide', 'io'),
              join_paths(meson.build_root(), 'src', 'libide', 'io'),

              join_paths(meson.source_root(), 'src', 'libide', 'threading'),
              join_paths(meson.build_root(), 'src', 'libide', 'threading'),

              join_paths(meson.source_root(), 'src', 'libide', 'code'),
              join_paths(meson.build_root(), 'src', 'libide', 'code'),

              join_paths(meson.source_root(), 'src', 'libide', 'foundry'),
              join_paths(meson.build_root(), 'src', 'libide', 'foundry'),

              join_paths(meson.source_root(), 'src', 'libide', 'sourceview'),
              join_paths(meson.build_root(), 'src', 'libide', 'sourceview'),

              join_paths(meson.source_root(), 'src', 'libide', 'editor'),
              join_paths(meson.build_root(), 'src', 'libide', 'editor'),

              join_paths(meson.source_root(), 'src', 'libide', 'vcs'),
              join_paths(meson.build_root(), 'src', 'libide', 'vcs'),

              join_paths(meson.source_root(), 'src', 'libide', 'debugger'),
              join_paths(meson.build_root(), 'src', 'libide', 'debugger'),

              join_paths(meson.source_root(), 'src', 'libide', 'greeter'),
              join_paths(meson.build_root(), 'src', 'libide', 'greeter'),

              join_paths(meson.source_root(), 'src', 'libide', 'gui'),
              join_paths(meson.build_root(), 'src', 'libide', 'gui'),

              join_paths(meson.source_root(), 'src', 'libide', 'lsp'),
              join_paths(meson.build_root(), 'src', 'libide', 'lsp'),

              join_paths(meson.source_root(), 'src', 'libide', 'plugins'),
              join_paths(meson.build_root(), 'src', 'libide', 'plugins'),

              join_paths(meson.source_root(), 'src', 'libide', 'search'),
              join_paths(meson.build_root(), 'src', 'libide', 'search'),

              join_paths(meson.source_root(), 'src', 'libide', 'terminal'),
              join_paths(meson.build_root(), 'src', 'libide', 'terminal'),

              join_paths(meson.source_root(), 'src', 'libide', 'tree'),
              join_paths(meson.build_root(), 'src', 'libide', 'tree'),
            ],

       dependencies: [ libide_gtk_doc_dep ],
  gobject_typesfile: 'libide.types',
          scan_args: [
            '--rebuild-types',
            '--ignore-decorators=_IDE_EXTERN',
          ],
     ignore_headers: private_headers,
      content_files: gnome_builder_public_sources + gnome_builder_public_headers,
             c_args: libide_args,

       fixxref_args: [
         '--html-dir=@0@'.format(docpath),
         '--extra-dir=@0@'.format(cairo_docpath),
         '--extra-dir=@0@'.format(gi_docpath),
         '--extra-dir=@0@'.format(glib_docpath),
         '--extra-dir=@0@'.format(gtk_docpath),
         '--extra-dir=@0@'.format(json_docpath),
         '--extra-dir=@0@'.format(libdazzle_docpath),
         '--extra-dir=@0@'.format(vte_docpath),
       ],
            install: true)


endif
