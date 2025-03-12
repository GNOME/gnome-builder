libide_projects_header_subdir = join_paths(libide_header_subdir, 'projects')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_projects_public_headers = [
  'ide-doap.h',
  'ide-doap-person.h',
  'ide-project.h',
  'ide-project-info.h',
  'ide-project-file.h',
  'ide-projects-global.h',
  'ide-project-template.h',
  'ide-project-tree-addin.h',
  'ide-recent-projects.h',
  'ide-similar-file-locator.h',
  'ide-template-base.h',
  'ide-template-input.h',
  'ide-template-locator.h',
  'ide-template-provider.h',
  'libide-projects.h',
]

install_headers(libide_projects_public_headers, subdir: libide_projects_header_subdir)

#
# Sources
#

libide_projects_private_headers = [
  'ide-project-info-private.h',
  'xml-reader-private.h',
]

libide_projects_private_sources = [ 'xml-reader.c', ]

libide_projects_public_sources = [
  'ide-doap.c',
  'ide-doap-person.c',
  'ide-project.c',
  'ide-project-info.c',
  'ide-project-file.c',
  'ide-projects-global.c',
  'ide-project-template.c',
  'ide-project-tree-addin.c',
  'ide-recent-projects.c',
  'ide-similar-file-locator.c',
  'ide-template-base.c',
  'ide-template-input.c',
  'ide-template-locator.c',
  'ide-template-provider.c',
]

libide_projects_resources = gnome.compile_resources(
  'ide-projects-resources',
  'libide-projects.gresource.xml',
  c_name: 'ide_projects',
)

libide_projects_sources = libide_projects_public_sources + libide_projects_private_sources + libide_projects_resources

#
# Dependencies
#

libide_projects_deps = [
  libgio_dep,
  libgtk_dep,
  libtemplate_glib_dep,
  libxml2_dep,

  libide_code_dep,
  libide_core_dep,
  libide_io_dep,
  libide_threading_dep,
  libide_tree_dep,
  libide_vcs_dep,
]

#
# Library Definitions
#

libide_projects = static_library('ide-projects-' + libide_api_version, libide_projects_sources,
   dependencies: libide_projects_deps,
         c_args: libide_args + release_args + ['-DIDE_PROJECTS_COMPILATION'],
)

libide_projects_dep = declare_dependency(
              sources: libide_projects_private_headers,
         dependencies: libide_projects_deps,
            link_with: libide_projects,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_projects_public_sources)
gnome_builder_public_headers += files(libide_projects_public_headers)
gnome_builder_include_subdirs += libide_projects_header_subdir
gnome_builder_gir_extra_args += '-DIDE_PROJECTS_COMPILATION'
gnome_builder_main_headers += 'libide-projects.h'
