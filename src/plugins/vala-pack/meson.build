if get_option('with_vala_pack')

if not get_option('with_vapi')
#  error('You must enable VAPI generation to build the Vala pack')
endif

add_languages('vala')

libvala_options = [
  ['libvala-0.40', ''],
  ['libvala-0.38', ''],
  ['libvala-0.36', ''],
  ['libvala-0.34', ''],
]

libvala_found = false
foreach v: libvala_options
  if not libvala_found or not libvala.found()
    if v[1] != ''
      libvala = dependency(v[0], version: v[1], required: false)
    else
      libvala = dependency(v[0], required: false)
    endif
    libvala_found = libvala.found()
    libvala_version = v[0].split('-')[1]
  endif
endforeach

if not libvala_found
  error('Failed to find suitable libvala')
endif

vala_pack_sources = [
  'config.vapi',
  'ide-vala-service.vala',
  'ide-vala-completion.vala',
  'ide-vala-completion-item.vala',
  'ide-vala-completion-provider.vala',
  'ide-vala-diagnostics.vala',
  'ide-vala-diagnostic-provider.vala',
  'ide-vala-indenter.vala',
  'ide-vala-index.vala',
  'ide-vala-locator.vala',
  'ide-vala-pipeline-addin.vala',
  'ide-vala-preferences-addin.vala',
  'ide-vala-source-file.vala',
  'ide-vala-symbol-resolver.vala',
  'ide-vala-symbol-tree.vala',
  'vala-pack-plugin.vala',
]

vala_pack_deps = [
  libvala,
  libide_vapi,
  libpeas_dep,
  libide_plugin_dep,
]

shared_module('vala-pack-plugin', vala_pack_sources,
     link_args: gnome_builder_plugins_link_args,
  link_depends: gnome_builder_plugins_link_deps,
  dependencies: vala_pack_deps,
       install: true,
   install_dir: plugindir,
 install_rpath: pkglibdir_abs,

     vala_args: [ '--target-glib=2.52',
                  '--pkg=posix',
                  '--pkg=libpeas-1.0',
                  '--pkg=gtksourceview-3.0',
                  '--pkg=gio-2.0',
                  '--pkg=libvala-' + libvala_version,
                  '--pkg=libdazzle-1.0',
                ],
        c_args: [ '-DVALA_VERSION="@0@"'.format(libvala_version),
                  '-DLOG_DOMAIN="vala-pack-plugin"',
                  '-DGETTEXT_PACKAGE="gnome-builder"',
                  '-DPACKAGE_DATADIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'gnome-builder')),
                ],
)

configure_file(
          input: 'vala-pack.plugin',
         output: 'vala-pack.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
