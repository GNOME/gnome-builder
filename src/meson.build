exe_link_args = [ '-pie' ]
exe_c_args = [ '-fPIE' ]

subdir('libeditorconfig')
subdir('gstyle')
subdir('libide')
subdir('plugins')
subdir('tests')

gnome_builder_sources = [
  'main.c',
  'bug-buddy.c',
  'bug-buddy.h',
]

executable('gnome-builder', gnome_builder_sources,
           gui_app: true,
           install: true,
            c_args: exe_c_args + release_args,
         link_args: exe_link_args,
     install_rpath: pkglibdir_abs,
      dependencies: gnome_builder_plugins_deps + [libide_dep, gnome_builder_plugins_dep],
)

if get_option('fusermount_wrapper')

  executable('fusermount', ['fusermount-wrapper.c'],
             gui_app: false,
             install: true,
         install_dir: get_option('bindir'),
              c_args: exe_c_args + release_args,
           link_args: exe_link_args,
       install_rpath: pkglibdir_abs,
        dependencies: [libide_deps, libide_dep],
  )

endif
