if get_option('with_docs')

conf_py_data = configuration_data()
conf_py_data.set_quoted('VERSION', meson.project_version())

configure_file(
         input: 'conf.py.in',
        output: 'conf.py',
 configuration: conf_py_data,
)

sphinx = find_program(['sphinx-build-3', 'sphinx-build'], required: false)

if sphinx.found()
    custom_target('en user documentation',
                  command: [sphinx, '-b', 'html', '-c', meson.current_build_dir(), meson.current_source_dir(), '@OUTPUT@'],
                   output: 'en',
         build_by_default: true)


    install_subdir(meson.current_build_dir() + '/en', install_dir: pkgdocdir_abs)
endif

endif
