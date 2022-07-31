test_tweaks = executable('test-tweaks', 'test-tweaks.c',
  dependencies: [libide_tweaks_dep, libgtksource_dep],
)

test_tweaks_runs = [
  # name    subdir   reference       input files
  ['test1', 'test1', 'expected.xml', ['0.ui', '1.ui', '2.ui']],
  ['test2', 'test2', 'expected.xml', ['0.ui', '1.ui']],
]

foreach test_tweaks_run: test_tweaks_runs
  test_name = test_tweaks_run.get(0)
  test_subdir = join_paths(meson.current_source_dir(), test_tweaks_run.get(1))
  test_expected = join_paths(test_subdir, test_tweaks_run.get(2))
  test_sources = []
  foreach test_tweaks_input: test_tweaks_run.get(3)
    test_sources += join_paths(test_subdir, test_tweaks_input)
  endforeach
  test('tweaks-@0@'.format(test_name), test_tweaks, args: ['--expected', test_expected]+test_sources)
endforeach
