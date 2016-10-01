libsearch_sources = [
  'fuzzy.c',
  'trie.c',
]

libsearch_deps = [
  dependency('gobject-2.0'),
]

libsearch = shared_library('search', libsearch_sources,
  dependencies: libsearch_deps,
  version: '0.0.0',
  install: true,
  install_dir: pkglibdir,
)

libsearch_dep = declare_dependency(
  link_with: libsearch,
  dependencies: libsearch_deps,
  include_directories: include_directories('.'),
)
