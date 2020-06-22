if get_option('help')

sphinx = find_program(['sphinx-build-3', 'sphinx-build'], required: true)

custom_target('en user documentation',
           command: [sphinx, '-b', 'html', '-c', meson.current_source_dir(), meson.current_source_dir(), '@OUTPUT@'],
            output: 'en',
  build_by_default: true
)

install_subdir(meson.current_build_dir() + '/en', install_dir: pkgdocdir_abs, exclude_directories: '.doctrees')

endif
