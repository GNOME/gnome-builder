if get_option('with_python_pack')

python_pack_sources = [
  'ide-python-indenter.c',
  'ide-python-indenter.h',
  'ide-python-format-provider.c',
  'ide-python-format-provider.h',
  'python-pack-plugin.c',
]

shared_module('python-pack-plugin', python_pack_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('python-pack.plugin', install_dir: plugindir)

endif
