schema_dir = join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
schema_files = []

# Perhaps this should be moved to the plugin
ctags_path = get_option('ctags_path')
if ctags_path == ''
  ctags_path = 'ctags'
  ctags = find_program('ctags-exuberant',
                       'exctags',
                       'ectags',
                       'ctags',
                       required: false)
  if ctags.found()
    ctags_path = ctags.path()
  endif
endif

schema_conf = configuration_data()
schema_conf.set('ECTAGS', ctags_path)
schema_conf.set('APP_ID', app_id)
schema_conf.set('APP_ID_LOWER', app_id_lower)
schema_conf.set('APP_PATH', app_path)
schema_conf.set('APP_PATH_LOWER', app_path_lower)
schema_conf.set('GETTEXT_DOMAIN', 'gnome-builder')

schemas = [
  'org.gnome.builder.code-insight.gschema.xml.in',
  'org.gnome.builder.build.gschema.xml.in',
  'org.gnome.builder.editor.gschema.xml.in',
  'org.gnome.builder.editor.language.gschema.xml.in',
  'org.gnome.builder.extension-type.gschema.xml.in',
  'org.gnome.builder.gschema.xml.in',
  'org.gnome.builder.plugin.gschema.xml.in',
  'org.gnome.builder.project.gschema.xml.in',
  'org.gnome.builder.project-tree.gschema.xml.in',
  'org.gnome.builder.terminal.gschema.xml.in',
  'org.gnome.builder.workbench.gschema.xml.in',
]

# HACK: We need access to all schemas when running tests, so we
#       need to copy them to the build dir (since the .in file
#       will also end up there).

foreach schema : schemas
  schema_files += [configure_file(
          input: schema,
         output: '@0@.xml'.format(schema.split('.xml.in')[0]),
  configuration: schema_conf,
        install: true,
    install_dir: schema_dir,
  )]
endforeach

gschemas_compiled = custom_target('gschemas.compiled',
            output: 'gschemas.compiled',
           command: ['glib-compile-schemas', meson.current_build_dir()],
      depend_files: schema_files,
  build_by_default: true,
           install: false,
)
