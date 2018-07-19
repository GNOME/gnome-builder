if get_option('with_vala_pack')

if not get_option('with_vapi')
#  error('You must enable VAPI generation to build the Vala pack')
endif

add_languages('vala')

valac = meson.get_compiler('vala')
libvala_version = run_command(valac.cmd_array()[0], '--api-version').stdout().strip()
libvala = dependency('libvala-@0@'.format(libvala_version))
vala_config_dep = meson.get_compiler('vala').find_library('config', dirs: join_paths(meson.current_source_dir()))

vala_pack_sources = [
  'ide-vala-client.vala',
  'ide-vala-code-indexer.vala',
  'ide-vala-completion-item.vala',
  'ide-vala-completion-provider.vala',
  'ide-vala-diagnostic-provider.vala',
  'ide-vala-indenter.vala',
  'ide-vala-pipeline-addin.vala',
  'ide-vala-preferences-addin.vala',
  'ide-vala-symbol-resolver.vala',
  'ide-vala-symbol-tree.vala',
  'vala-pack-plugin.vala',
]

vala_pack_deps = [
  libide_vapi,
  libpeas_dep,
  libide_plugin_dep,
  vala_config_dep
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
                  '--pkg=gtksourceview-4',
                  '--pkg=gio-2.0',
                  '--pkg=gio-unix-2.0',
                  '--pkg=jsonrpc-glib-1.0',
                  '--pkg=libdazzle-1.0',
                ],
        c_args: [ '-DVALA_VERSION="@0@"'.format(libvala_version),
                  '-DLOG_DOMAIN="vala-pack-plugin"',
                  '-DGETTEXT_PACKAGE="gnome-builder"',
                  '-DPACKAGE_LIBEXECDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('libexecdir'))),
                  '-DPACKAGE_DATADIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'gnome-builder')),
		  '-Wno-incompatible-pointer-types',
                ],
)

subdir('lang-server')

configure_file(
          input: 'vala-pack.plugin',
         output: 'vala-pack.plugin',
           copy: true,
        install: true,
    install_dir: plugindir,
)

endif
