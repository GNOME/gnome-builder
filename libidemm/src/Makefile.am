if ENABLE_IDEMM

binding_name    = idemm
wrap_init_flags = --namespace=Ide --parent_dir=idemm
codegen_srcdir  = $(top_srcdir)/libidemm/tools

include $(srcdir)/filelist.am
include $(top_srcdir)/libidemm/tools/m4/filelist.am

files_defs = $(idemm_files_defs)
files_ccg  = $(idemm_files_ccg)
files_hg   = $(idemm_files_hg)

include $(top_srcdir)/libidemm/generate-binding.am

endif
