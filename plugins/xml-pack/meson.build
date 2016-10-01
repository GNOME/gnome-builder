if get_option('with_xml_pack')

xml_pack_sources = [
  'ide-xml-analysis.c',
  'ide-xml-analysis.h',
  'ide-xml-diagnostic-provider.c',
  'ide-xml-diagnostic-provider.h',
  'ide-xml-highlighter.c',
  'ide-xml-highlighter.h',
  'ide-xml-indenter.c',
  'ide-xml-indenter.h',
  'ide-xml-sax.c',
  'ide-xml-sax.h',
  'ide-xml-service.c',
  'ide-xml-service.h',
  'ide-xml-stack.c',
  'ide-xml-stack.h',
  'ide-xml-symbol-node.c',
  'ide-xml-symbol-node.h',
  'ide-xml-symbol-resolver.c',
  'ide-xml-symbol-resolver.h',
  'ide-xml-symbol-tree.c',
  'ide-xml-symbol-tree.h',
  'ide-xml-tree-builder.c',
  'ide-xml-tree-builder.h',
  'ide-xml-tree-builder-generic.c',
  'ide-xml-tree-builder-generic.h',
  'ide-xml-tree-builder-ui.c',
  'ide-xml-tree-builder-ui.h',
  'ide-xml-tree-builder-utils.c',
  'ide-xml-tree-builder-utils-private.h',
  'ide-xml.c',
  'ide-xml.h',
  'xml-pack-plugin.c',
]

xml_pack_deps = plugin_deps + [
  libxml_dep,
]

shared_module('xml-pack-plugin', xml_pack_sources,
  dependencies: xml_pack_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('xml-pack.plugin', install_dir: plugindir)

endif
