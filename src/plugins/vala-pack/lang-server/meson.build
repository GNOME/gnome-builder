gnome_builder_vala_sources = files(
  'gnome-builder-vala.vala',
  'ide-vala-index.vala',
  'ide-vala-diagnostics.vala'
)

gnome_builder_vala_deps = [
  libvala,
  libide_vapi,
  libgiounix_dep,
  libjsonrpc_glib_dep,
  libide_dep,
  vala_config_dep
]

executable('gnome-builder-vala', gnome_builder_vala_sources,
      dependencies: gnome_builder_vala_deps,
           gui_app: false,
           install: true,
       install_dir: get_option('libexecdir'),
         link_args: exe_link_args,
     install_rpath: pkglibdir_abs,

     vala_args: [ '--target-glib=2.52',
                  '--pkg=posix',
                  '--pkg=gio-2.0',
                  '--pkg=gio-unix-2.0',
                  '--pkg=libvala-' + libvala_version,
                  '--pkg=jsonrpc-glib-1.0',
                ],
        c_args: exe_c_args +
                [ '-DVALA_VERSION="@0@"'.format(libvala_version),
                  '-DLOG_DOMAIN="vala-pack-plugin"',
                  '-DGETTEXT_PACKAGE="gnome-builder"',
                  '-DPACKAGE_DATADIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'gnome-builder')),
                  '-Wno-incompatible-pointer-types',
                ],
)
