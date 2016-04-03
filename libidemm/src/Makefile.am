binding_name    = libidemm
wrap_init_flags = --namespace=Ide --parent_dir=libidemm
codegen_srcdir  = $(top_srcdir)/libidemm/tools

include $(srcdir)/filelist.am
include $(top_srcdir)/libidemm/tools/m4/filelist.am

files_defs = $(libidemm_files_defs)
files_ccg  = $(libidemm_files_ccg)
files_hg   = $(libidemm_files_hg)

include $(top_srcdir)/libidemm/generate-binding.am
