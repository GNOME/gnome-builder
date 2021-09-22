# Appdata file.
appdata_file_untranslated = vcs_tag(
  command: ['git', 'rev-parse', 'HEAD'],
  input: 'org.gnome.Builder.appdata.xml.in.in',
  output: 'org.gnome.Builder.appdata.xml.in',
)

appdata_file = i18n.merge_file(
  input: appdata_file_untranslated[0],
  output: 'org.gnome.Builder.appdata.xml',
  po_dir: '../../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  validate_args = ['validate-relax', appdata_file]
if not get_option('network_tests')
  validate_args += '--nonet'
endif
  test('Validate appstream file', appstream_util, args: validate_args)
endif
