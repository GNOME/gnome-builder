if get_option('with_xml_pack')

xml_pack_resources = gnome.compile_resources(
  'xml-pack-resources',
  'xml-pack.gresource.xml',
  c_name: 'ide_xml'
)

xml_pack_sources = [
  'ide-xml-analysis.c',
  'ide-xml-completion-attributes.c',
  'ide-xml-completion-values.c',
  'ide-xml-completion-provider.c',
  'ide-xml-detail.c',
  'ide-xml-diagnostic-provider.c',
  'ide-xml-hash-table.c',
  'ide-xml-highlighter.c',
  'ide-xml-indenter.c',
  'ide-xml-parser.c',
  'ide-xml-parser-generic.c',
  'ide-xml-parser-ui.c',
  'ide-xml-path.c',
  'ide-xml-position.c',
  'ide-xml-proposal.c',
  'ide-xml-rng-define.c',
  'ide-xml-rng-grammar.c',
  'ide-xml-rng-parser.c',
  'ide-xml-sax.c',
  'ide-xml-schema.c',
  'ide-xml-schema-cache-entry.c',
  'ide-xml-service.c',
  'ide-xml-stack.c',
  'ide-xml-symbol-node.c',
  'ide-xml-symbol-resolver.c',
  'ide-xml-symbol-tree.c',
  'ide-xml-tree-builder.c',
  'ide-xml-tree-builder-utils.c',
  'ide-xml-utils.c',
  'ide-xml-validator.c',
  'ide-xml.c',
  'xml-pack-plugin.c',
]

gnome_builder_plugins_sources += files(xml_pack_sources)
gnome_builder_plugins_sources += xml_pack_resources[0]

endif