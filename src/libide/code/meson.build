libide_code_header_dir = join_paths(libide_header_dir, 'code')
libide_code_header_subdir = join_paths(libide_header_subdir, 'code')

libide_code_generated_sources = []
libide_code_generated_headers = []
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_code_private_headers = [
  'cjhtextregionbtree.h',
  'cjhtextregionprivate.h',
  'ide-buffer-private.h',
  'ide-doc-seq-private.h',
  'ide-gsettings-file-settings.h',
  'ide-language-defaults.h',
  'ide-text-edit-private.h',
  'ide-unsaved-file-private.h',
]

libide_code_public_headers = [
  'ide-buffer-addin.h',
  'ide-buffer-change-monitor.h',
  'ide-buffer.h',
  'ide-buffer-manager.h',
  'ide-code-action.h',
  'ide-code-action-provider.h',
  'ide-code-index-entries.h',
  'ide-code-index-entry.h',
  'ide-code-indexer.h',
  'ide-code-types.h',
  'ide-diagnostic.h',
  'ide-diagnostic-provider.h',
  'ide-diagnostics.h',
  'ide-diagnostics-manager.h',
  'ide-file-settings.h',
  'ide-file-settings.defs',
  'ide-formatter.h',
  'ide-formatter-options.h',
  'ide-highlight-engine.h',
  'ide-highlighter.h',
  'ide-highlight-index.h',
  'ide-indent-style.h',
  'ide-language.h',
  'ide-location.h',
  'ide-range.h',
  'ide-rename-provider.h',
  'ide-source-iter.h',
  'ide-source-style-scheme.h',
  'ide-spaces-style.h',
  'ide-symbol.h',
  'ide-symbol-node.h',
  'ide-symbol-resolver.h',
  'ide-symbol-tree.h',
  'ide-text-edit.h',
  'ide-text-iter.h',
  'ide-unsaved-file.h',
  'ide-unsaved-files.h',
  'libide-code.h',
]

libide_code_enum_headers = [
  'ide-buffer.h',
  'ide-buffer-manager.h',
  'ide-diagnostic.h',
  'ide-indent-style.h',
  'ide-spaces-style.h',
  'ide-symbol.h',
]

install_headers(libide_code_public_headers, subdir: libide_code_header_subdir)

#
# Sources
#

libide_code_private_sources = [
  'cjhtextregion.c',
  'ide-doc-seq.c',
  'ide-gsettings-file-settings.c',
  'ide-language-defaults.c',
]

libide_code_public_sources = [
  'ide-buffer-addin.c',
  'ide-buffer.c',
  'ide-buffer-change-monitor.c',
  'ide-buffer-manager.c',
  'ide-code-global.c',
  'ide-code-action.c',
  'ide-code-action-provider.c',
  'ide-code-index-entries.c',
  'ide-code-index-entry.c',
  'ide-code-indexer.c',
  'ide-diagnostic.c',
  'ide-diagnostic-provider.c',
  'ide-diagnostics.c',
  'ide-diagnostics-manager.c',
  'ide-file-settings.c',
  'ide-formatter.c',
  'ide-formatter-options.c',
  'ide-highlight-engine.c',
  'ide-highlighter.c',
  'ide-highlight-index.c',
  'ide-language.c',
  'ide-location.c',
  'ide-range.c',
  'ide-rename-provider.c',
  'ide-source-iter.c',
  'ide-source-style-scheme.c',
  'ide-symbol.c',
  'ide-symbol-node.c',
  'ide-symbol-resolver.c',
  'ide-symbol-tree.c',
  'ide-text-edit.c',
  'ide-text-iter.c',
  'ide-unsaved-file.c',
  'ide-unsaved-files.c',
]

#
# Enum generation
#

libide_code_enums = gnome.mkenums_simple('ide-code-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_code_enum_headers,
  install_header: true,
     install_dir: libide_code_header_dir,
)
libide_code_generated_sources += [libide_code_enums[0]]
libide_code_generated_headers += [libide_code_enums[1]]

#
# Generated Resource Files
#

libide_code_resources = gnome.compile_resources(
  'ide-code-resources',
  'libide-code.gresource.xml',
  c_name: 'ide_code',
)
libide_code_generated_headers += [libide_code_resources[1]]
libide_code_generated_sources += libide_code_resources


#
# Dependencies
#

libide_code_deps = [
  libgio_dep,
  libgtk_dep,
  libgtksource_dep,
  libtemplate_glib_dep,

  libide_core_dep,
  libide_plugins_dep,
  libide_io_dep,
  libide_threading_dep,
]

#
# Library Definitions
#


libide_code = static_library('ide-code-' + libide_api_version,
                             libide_code_public_sources,
                             libide_code_private_sources,
                             libide_code_generated_sources,
                             libide_code_generated_headers,
   dependencies: libide_code_deps,
         c_args: libide_args + release_args + ['-DIDE_CODE_COMPILATION'],
)

libide_code_dep = declare_dependency(
              sources: libide_code_private_headers + libide_code_generated_headers,
         dependencies: libide_code_deps,
            link_with: libide_code,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_code_public_sources)
gnome_builder_public_headers += files(libide_code_public_headers)
gnome_builder_private_sources += files(libide_code_private_sources)
gnome_builder_private_headers += files(libide_code_private_headers)
gnome_builder_generated_headers += libide_code_generated_headers
gnome_builder_generated_sources += libide_code_generated_sources
gnome_builder_include_subdirs += libide_code_header_subdir
gnome_builder_gir_extra_args += '-DIDE_CODE_COMPILATION'
gnome_builder_main_headers += 'libide-code.h'
