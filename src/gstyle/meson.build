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
  libgstyle_resources,
]

libgstyle_deps = [
  libdazzle_dep,
  libgtk_dep,
  libm_dep,
  libxml2_dep,
]

libgstyle = static_library('gstyle-private', libgstyle_sources,
  dependencies: libgstyle_deps,
        c_args: hidden_visibility_args + release_args,
)

libgstyle_dep = declare_dependency(
           link_whole: libgstyle,
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

subdir('tests')
