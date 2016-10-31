{{name_}}_sources = [
  '{{if language == "c"}}main.c{{else if language == "cpp"}}main.cpp{{end}}',
]

{{name_}}_deps = [
  dependency('glib-2.0'),
]

executable('{{name}}', {{name_}}_sources,
  dependencies: {{name_}}_deps,
  install: true,
)
