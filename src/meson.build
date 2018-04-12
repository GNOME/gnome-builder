subdir('libeditorconfig')
subdir('gstyle')
subdir('libide')
subdir('plugins')
subdir('tests')

exe_link_args = [ '-pie' ]
exe_c_args = [ '-fPIE' ]

gnome_builder_sources = [
  'main.c',
  'bug-buddy.c',
  'bug-buddy.h',
]

executable('gnome-builder', gnome_builder_sources,
           gui_app: true,
           install: true,
            c_args: exe_c_args,
         link_args: exe_link_args,
     install_rpath: pkglibdir_abs,
      dependencies: gnome_builder_plugins_deps + [libide_dep, gnome_builder_plugins_dep],
)

# Don't install until we actually start using this
# install_dir: get_option('libexecdir'),
executable('ide-host-exec', ['host-exec.c'],
           install: false,
            c_args: exe_c_args,
         link_args: exe_link_args,
     install_rpath: pkglibdir_abs,
      dependencies: [libide_deps, libide_dep],
)
