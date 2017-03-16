sphinx_build = find_program('sphinx-build', required: false)

if sphinx_build.found()
  # This would need to be done at 'dist' time which doesn't exist for Meson yet
  run_target('sphinx-docs',
    command: [sphinx_build, meson.current_source_dir(), meson.source_root() + '/html']
  )

  run_target('sphinx-docs-pot',
    command: [sphinx_build, '-b', 'gettext', meson.current_source_dir(), 'locale']
  )
endif