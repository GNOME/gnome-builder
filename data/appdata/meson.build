metainfo_config = configuration_data()
metainfo_config.set('app_id', app_id)
metainfo_file_in = configure_file(
          input: 'org.gnome.Builder.metainfo.xml.in.in',
         output: 'org.gnome.Builder.metainfo.xml.in',
  configuration: metainfo_config,
)
metainfo_file = i18n.merge_file(
        input: metainfo_file_in,
       output: '@0@.metainfo.xml'.format(app_id),
       po_dir: '../../po',
      install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  validate_args = ['validate', metainfo_file]
if not get_option('network_tests')
  validate_args += '--no-net'
endif
  test('Validate appstream file', appstreamcli, args: validate_args)
endif
