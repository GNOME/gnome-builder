if get_option('plugin_xml_pack')

plugins_sources += files([
  'ide-xml-analysis.c',
  'ide-xml-completion-attributes.c',
  'ide-xml-completion-values.c',
  'ide-xml-completion-provider.c',
  'ide-xml-diagnostic-provider.c',
  'ide-xml-formatter.c',
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
])

plugin_xml_pack_resources = gnome.compile_resources(
  'gbp-xml-pack-resources',
  'xml-pack.gresource.xml',
  c_name: 'gbp_xml_pack',
)

plugins_sources += plugin_xml_pack_resources

endif
