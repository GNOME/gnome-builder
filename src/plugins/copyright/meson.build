if get_option('plugin_copyright')
  plugins_sources += files(
    'copyright-plugin.c',
    'gbp-copyright-buffer-addin.c',
    'gbp-copyright-preferences-addin.c',
  )

  install_data('org.gnome.builder.plugins.copyright.gschema.xml', install_dir: schema_dir)
endif
