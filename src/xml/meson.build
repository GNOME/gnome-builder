libxml_sources = [
  'xml-reader.c',
]

libxml2_dep = dependency('libxml-2.0', version: '>=2.9.0')

libxml_deps = [
  libgio_dep,
  libxml2_dep,
]

libxml = static_library('xml', libxml_sources,
  dependencies: libxml_deps,
  pic: true,
)

libxml_dep = declare_dependency(
  link_with: libxml,
  dependencies: libxml2_dep,
  include_directories: include_directories('.'),
)
