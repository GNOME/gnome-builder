# Appdata file
appdata_file = i18n.merge_file(
        input: configure_file(
                         input: 'org.gnome.Builder.appdata.xml.in.in',
                        output: 'org.gnome.Builder.appdata.xml.in',
                 configuration: config_h),
       output: '@@0@@.appdata.xml'.format(app_id),
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
