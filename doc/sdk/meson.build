subdir('xml')

private_headers = ['config.h']
foreach source : libide_private_sources
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
gsv_prefix = dependency('gtksourceview-3.0').get_pkgconfig_variable('prefix')
gsv_docpath = join_paths(gsv_prefix, 'share', 'gtk-doc', 'html')
vte_prefix = dependency('vte-2.91').get_pkgconfig_variable('prefix')
vte_docpath = join_paths(vte_prefix, 'share', 'vte-doc', 'html')

# Locate our directory for documentation
docpath = join_paths(get_option('datadir'), 'gtk-doc', 'html')

gnome.gtkdoc('libide',
           main_xml: 'libide-docs.sgml',
            src_dir: [
              join_paths(meson.source_root(), 'src', 'libide'),
              join_paths(meson.build_root(), 'src', 'libide'),
            ],

       dependencies: libide_dep,
  gobject_typesfile: 'libide.types',
          scan_args: [
            '--rebuild-types',
            '--ignore-decorators=_IDE_EXTERN',
          ],
     ignore_headers: private_headers,

       fixxref_args: [
         '--html-dir=@0@'.format(docpath),
         '--extra-dir=@0@'.format(join_paths(cairo_docpath, 'cairo')),
         '--extra-dir=@0@'.format(join_paths(gi_docpath, 'gi')),
         '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
         '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
         '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
         '--extra-dir=@0@'.format(join_paths(gtk_docpath, 'gdk3')),
         '--extra-dir=@0@'.format(join_paths(gtk_docpath, 'gtk3')),
         '--extra-dir=@0@'.format(join_paths(gtk_docpath, 'gtksourceview-3.0')),
         '--extra-dir=@0@'.format(join_paths(libdazzle_docpath, 'libdazzle')),
         '--extra-dir=@0@'.format(join_paths(json_docpath, 'json-glib')),
         '--extra-dir=@0@'.format(join_paths(vte_docpath, 'vte-2.91')),
         '--extra-dir=@0@'.format(json_docpath),
       ],
            install: true)

