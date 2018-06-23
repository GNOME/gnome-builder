libgstyle_headers = [
  'gstyle-animation.h',
  'gstyle-cielab.h',
  'gstyle-color.h',
  'gstyle-color-component.h',
  'gstyle-color-convert.h',
  'gstyle-color-filter.h',
  'gstyle-color-item.h',
  'gstyle-color-panel.h',
  'gstyle-color-panel-private.h',
  'gstyle-color-panel-actions.h',
  'gstyle-color-plane.h',
  'gstyle-color-predefined.h',
  'gstyle-color-scale.h',
  'gstyle-color-widget.h',
  'gstyle-color-widget-actions.h',
  'gstyle-colorlexer.h',
  'gstyle-css-provider.h',
  'gstyle-eyedropper.h',
  'gstyle-hsv.h',
  'gstyle-palette.h',
  'gstyle-palette-widget.h',
  'gstyle-private.h',
  'gstyle-rename-popover.h',
  'gstyle-revealer.h',
  'gstyle-slidein.h',
  'gstyle-types.h',
  'gstyle-utils.h',
  'gstyle-xyz.h',
]

libgstyle_header_dir = join_paths(pkgincludedir, 'gstyle')

install_headers(libgstyle_headers,
  install_dir: libgstyle_header_dir,
)

libgstyle_resources = gnome.compile_resources('gstyle-resources',
  'gstyle.gresource.xml',
  c_name: 'gstyle',
)

libgstyle_sources = [
  'gstyle-animation.c',
  'gstyle-cielab.c',
  'gstyle-color.c',
  'gstyle-color-component.c',
  'gstyle-color-convert.c',
  'gstyle-color-filter.c',
  'gstyle-color-item.c',
  'gstyle-color-panel.c',
  'gstyle-color-panel-actions.c',
  'gstyle-color-plane.c',
  'gstyle-color-scale.c',
  'gstyle-color-widget.c',
  'gstyle-color-widget-actions.c',
  'gstyle-colorlexer.c',
  'gstyle-css-provider.c',
  'gstyle-eyedropper.c',
  'gstyle-hsv.c',
  'gstyle-palette.c',
  'gstyle-palette-widget.c',
  'gstyle-rename-popover.c',
  'gstyle-revealer.c',
  'gstyle-slidein.c',
  'gstyle-utils.c',
  'gstyle-xyz.c',
  libgstyle_resources[0],
]

libgstyle_deps = [
  libdazzle_dep,
  libgtk_dep,
  libm_dep,
  libxml2_dep,
]

libgstyle_link_args = []
if ld_supports_version_script
libgstyle_link_args += ['-Wl,--version-script=' + join_paths(meson.current_source_dir(), 'gstyle.map')]
endif

libgstyle = shared_library('gstyle-private', libgstyle_sources,
  dependencies: libgstyle_deps,
     link_args: libgstyle_link_args,
  link_depends: 'gstyle.map',
       version: '0.0.0',
       install: true,
   install_dir: pkglibdir,
)

libgstyle_dep = declare_dependency(
  link_with: libgstyle,
  dependencies: libgstyle_deps,
  include_directories: include_directories('.'),
)

libgstyle_introspection_sources = [
  'gstyle-cielab.h',
  'gstyle-cielab.c',
  'gstyle-color.h',
  'gstyle-color.c',
  'gstyle-color-component.h',
  'gstyle-color-component.c',
  'gstyle-color-convert.h',
  'gstyle-color-convert.c',
  'gstyle-color-filter.h',
  'gstyle-color-filter.c',
  'gstyle-color-item.h',
  'gstyle-color-item.c',
  'gstyle-color-panel.h',
  'gstyle-color-panel.c',
  'gstyle-color-plane.h',
  'gstyle-color-plane.c',
  'gstyle-color-scale.h',
  'gstyle-color-scale.c',
  'gstyle-color-widget.h',
  'gstyle-color-widget.c',
  'gstyle-eyedropper.h',
  'gstyle-eyedropper.c',
  'gstyle-hsv.h',
  'gstyle-hsv.c',
  'gstyle-palette.h',
  'gstyle-palette.c',
  'gstyle-palette-widget.h',
  'gstyle-palette-widget.c',
  'gstyle-slidein.h',
  'gstyle-slidein.c',
  'gstyle-xyz.h',
  'gstyle-xyz.c',
]

libgstyle_gir = gnome.generate_gir(libgstyle,
              sources: libgstyle_introspection_sources,
            nsversion: '1.0',
            namespace: 'Gstyle',
        symbol_prefix: 'gstyle',
    identifier_prefix: 'Gstyle',
             includes: ['Gdk-3.0', 'Gio-2.0', 'Gtk-3.0', 'GtkSource-4'],
              install: true,
      install_dir_gir: pkggirdir,
  install_dir_typelib: pkgtypelibdir,
           extra_args: [ '--c-include=gstyle-private.h' ],
)

if get_option('with_vapi')

  libgstyle_vapi = gnome.generate_vapi('gstyle-private',
        sources: libgstyle_gir[0],
       packages: ['gio-2.0', 'gtk+-3.0', 'gtksourceview-3.0'],
        install: true,
    install_dir: pkgvapidir,
  )

endif

subdir('tests')
