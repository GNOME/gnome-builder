appdata_config = configuration_data()
appdata_config.set('app_id', app_id)
appdata_file_in = configure_file(
          input: 'org.gnome.Builder.appdata.xml.in.in',
         output: 'org.gnome.Builder.appdata.xml.in',
  configuration: appdata_config,
)
appdata_file = i18n.merge_file(
        input: appdata_file_in,
       output: '@0@.appdata.xml'.format(app_id),
       po_dir: '../../po',
      install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  validate_args = ['validate', appdata_file]
if not get_option('network_tests')
  validate_args += '--no-net'
endif
  test('Validate appstream file', appstreamcli, args: validate_args)
endif
