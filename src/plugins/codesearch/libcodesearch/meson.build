libcodesearch_deps = [
  libgio_dep,
  libdex_dep,
]

libcodesearch_sources = [
  'code-index.c',
  'code-line-reader.c',
  'code-query.c',
  'code-query-spec.c',
  'code-result.c',
  'code-result-set.c',
]

libcodesearch_static = static_library('libcodesearch',
  libcodesearch_sources,
  dependencies: libcodesearch_deps,
)

libcodesearch_static_dep = declare_dependency(
         dependencies: libcodesearch_deps,
            link_with: libcodesearch_static,
  include_directories: include_directories('.'),
)
