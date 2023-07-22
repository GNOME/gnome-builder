if get_option('plugin_sysprof')

sysprof_req_version = '3.46.0'
sysprof_req = '>= @0@'.format(sysprof_req_version)

libsysprof_dep = dependency('sysprof-6', version: sysprof_req)

plugins_deps += [
  libsysprof_dep,
]

ipc_sysprof_src = gnome.gdbus_codegen('ipc-sysprof',
           sources: 'org.gnome.Sysprof.Agent.xml',
  interface_prefix: 'org.gnome.Sysprof.',
         namespace: 'Ipc',
        extra_args: ['--glib-min-required=@0@'.format(glib_req_version)],
)
plugins_sources += ipc_sysprof_src

plugins_sources += files([
  'sysprof-plugin.c',
  'gbp-sysprof-tool.c',
  'gbp-sysprof-tweaks-addin.c',
  'gbp-sysprof-workbench-addin.c',
])

plugin_sysprof_resources = gnome.compile_resources(
  'sysprof-resources',
  'sysprof.gresource.xml',
  c_name: 'gbp_sysprof',
)

plugins_sources += plugin_sysprof_resources

install_data(['org.gnome.builder.sysprof.gschema.xml'], install_dir: schema_dir)

endif
