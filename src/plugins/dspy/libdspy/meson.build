libdspy_sources = [
  'dspy-connection.c',
  'dspy-connection-button.c',
  'dspy-introspection-model.c',
  'dspy-method-invocation.c',
  'dspy-method-view.c',
  'dspy-name.c',
  'dspy-name-marquee.c',
  'dspy-name-row.c',
  'dspy-names-model.c',
  'dspy-node.c',
  'dspy-signature.c',
  'dspy-tree-view.c',
  'dspy-view.c',
]

libdspy_deps = [
  libgio_dep,
  libgtk_dep,
  libdazzle_dep,
]

libdspy_sources += gnome.compile_resources('libdspy-resources', 'libdspy.gresource.xml',
  c_name: 'libdspy'
)

libdspy = static_library('libdspy', libdspy_sources,
         dependencies: libdspy_deps,
)

libdspy_dep = declare_dependency(
         dependencies: libdspy_deps,
            link_with: [ libdspy ],
  include_directories: include_directories('.'),
)
