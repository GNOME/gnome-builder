if get_option('plugin_vala')

add_languages('vala')

valac = meson.get_compiler('vala')
libvala_version = run_command(valac.cmd_array()[0], '--api-version').stdout().strip()
libvala = dependency('libvala-@0@'.format(libvala_version))

vala_sources = [
  'valaconfig.vapi',
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

vala_deps = [
  libpeas_dep,
  libide_vapi,
  libgtksource_dep,
  libvte_dep,
  libdazzle_dep,
  libtemplate_glib_dep,
  libjsonrpc_glib_dep,
  libgiounix_dep,
]

shared_module('plugin-vala-pack', vala_sources,
         dependencies: vala_deps,
              install: true,
          install_dir: plugindir,
        install_rpath: pkglibdir_abs,
  include_directories: [include_directories('.')] + libide_include_directories,
            vala_args: [ '--target-glib=2.52',
                         '--pkg=posix',
                         '--pkg=libpeas-1.0',
                         '--pkg=gtksourceview-4',
                         '--pkg=gio-2.0',
                         '--pkg=gio-unix-2.0',
                         '--pkg=libdazzle-1.0',
                         '--pkg=template-glib-1.0',
                         '--pkg=vte-2.91',
                         '--pkg=jsonrpc-glib-1.0',
                       ],
               c_args: [ '-DVALA_VERSION="@0@"'.format(libvala_version),
                         '-DLOG_DOMAIN="vala-pack"',
                         '-DGETTEXT_PACKAGE="gnome-builder"',
                         '-DPACKAGE_DATADIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'gnome-builder')),
                         '-DPACKAGE_LIBEXECDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('libexecdir'))),
                         '-Wno-incompatible-pointer-types',
                       ],
)

subdir('lang-server')

configure_file(
          input: 'vala-pack.plugin',
         output: 'vala-pack.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

endif
