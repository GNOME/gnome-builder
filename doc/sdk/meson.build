if get_option('docs')

gidocgen = find_program('gi-docgen')

docs_dir = join_paths(get_option('datadir'), 'doc')

toml_conf = configuration_data()
toml_conf.set('version', libide_api_version)

libide_toml = configure_file(
    input: 'libide.toml.in',
    output: 'libide.toml',
    configuration: toml_conf,
    install: true,
    install_dir: docs_dir / 'libide',
)

custom_target('libide-doc',
    input: [ libide_toml, libide_gir[0] ],
    output: 'libide',
    command: [
      gidocgen,
      'generate',
      '--quiet',
      '--fatal-warnings',
      '--add-include-path=@0@'.format(meson.current_build_dir() / '../../src'),
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--no-namespace-dir',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    build_by_default: true,
    install: true,
    install_dir: docs_dir,
)
endif
