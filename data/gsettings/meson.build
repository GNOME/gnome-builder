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
    ctags_path = ctags.full_path()
  endif
endif

schema_conf = configuration_data()
schema_conf.set('ECTAGS', ctags_path)

schema_files += [configure_file(
          input: 'org.gnome.builder.code-insight.gschema.xml.in',
         output: 'org.gnome.builder.code-insight.gschema.xml',
  configuration: schema_conf,
        install: true,
    install_dir: schema_dir,
)]

schemas = [
  'org.gnome.builder.build.gschema.xml',
  'org.gnome.builder.debug.gschema.xml',
  'org.gnome.builder.editor.gschema.xml',
  'org.gnome.builder.editor.language.gschema.xml',
  'org.gnome.builder.extension-type.gschema.xml',
  'org.gnome.builder.gschema.xml',
  'org.gnome.builder.plugin.gschema.xml',
  'org.gnome.builder.project.gschema.xml',
  'org.gnome.builder.project-tree.gschema.xml',
  'org.gnome.builder.spelling.gschema.xml',
  'org.gnome.builder.terminal.gschema.xml',
]

# HACK: We need access to all schemas when running tests, so we
#       need to copy them to the build dir (since the .in file
#       will also end up there).

foreach schema : schemas
  schema_files += [configure_file(
          input: schema,
         output: schema,
           copy: true,
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
