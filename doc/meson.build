if get_option('with_docs')

sphinx = find_program('sphinx-build-3', required: false)
if not sphinx.found()
    sphinx = find_program('sphinx-build', required: false)
endif

if sphinx.found()
    docs_sources = [
    ]

    custom_target('en user documentation',
                  command: [sphinx, '-b', 'html', meson.current_source_dir(), '@OUTPUT@'],
                    input: docs_sources,
                   output: 'en',
         build_by_default: true)

    install_subdir(meson.current_build_dir() + '/en', install_dir: pkgdocdir_abs)
endif

endif
