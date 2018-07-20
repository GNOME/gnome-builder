# Tests are added from Builder regular tests because of deps required.

gi_test_env = [
  'GB_IN_TREE_PLUGINS=1',
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'GSETTINGS_BACKEND=memory',
  'MALLOC_CHECK_=2',
#  'MALLOC_PERTURB_=$((${RANDOM:-256} % 256))',
]

gi_test_cflags = [
  '-DTEST_DATA_DIR="@0@/data/"'.format(meson.current_source_dir()),
]

libglib_dep = dependency('glib-2.0')
libgobject_dep = dependency('gobject-2.0')

gi_test_deps = [
  libglib_dep,
  libgobject_dep
]

gi_radix_tree_builder_deps = [
  libdazzle_dep,
  libglib_dep,
  libgobject_dep
]

gi_repository_deps = [
  libdazzle_dep,
  libide_dep,
  libpeas_dep,
  gnome_builder_plugins_export_all_dep
]

gi_radix_tree_builder_sources = [
  '../../../plugins/gi/radix-tree/ide-gi-flat-radix-tree.c',
  '../../../plugins/gi/radix-tree/ide-gi-flat-radix-tree.h',
  '../../../plugins/gi/radix-tree/ide-gi-radix-tree-builder.c',
  '../../../plugins/gi/radix-tree/ide-gi-radix-tree-builder.h',
]

gi_radix_tree_builder = static_library(
  'gi-radix-tree-builder',
  gi_radix_tree_builder_sources,
  dependencies: gi_radix_tree_builder_deps,
)

test_gi_radix_tree = executable('test-gi-radix-tree', 'test-gi-radix-tree.c',
  link_with: gi_radix_tree_builder,
  dependencies: gi_test_deps
)

test('test-gi-radix-tree', test_gi_radix_tree,
  env: gi_test_env
)


test_gi_repository_sources = [
  'test-gi-common.c',
  'test-gi-common.h',
  'test-gi-repository.c',
  'test-gi-utils.h',
  'test-gi-utils.c',
]

test_gi_repository = executable('test-gi-repository', test_gi_repository_sources,
  c_args: gi_test_cflags,
  dependencies: gi_repository_deps,
)

test('test-gi-repository', test_gi_repository,
  env: gi_test_env
)


test_gi_query_sources = [
  'test-gi-common.c',
  'test-gi-common.h',
  'test-gi-query.c',
  'test-gi-utils.h',
  'test-gi-utils.c',
]

test_gi_query = executable('test-gi-query', test_gi_query_sources,
  c_args: gi_test_cflags,
  dependencies: gi_repository_deps,
)

test('test-gi-query', test_gi_query,
  env: gi_test_env
)


test_gi_version_sources = [
  'test-gi-common.c',
  'test-gi-common.h',
  'test-gi-version.c',
  'test-gi-utils.h',
  'test-gi-utils.c',
]

test_gi_version = executable('test-gi-version', test_gi_version_sources,
  c_args: gi_test_cflags,
  dependencies: gi_repository_deps,
)

test('test-gi-version', test_gi_version,
  env: gi_test_env
)