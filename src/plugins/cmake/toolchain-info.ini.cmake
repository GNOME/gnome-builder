[general]
cpu=@CMAKE_SYSTEM_PROCESSOR@
system=@CMAKE_SYSTEM_NAME@

[binaries]
exe_wrapper=@CMAKE_CROSSCOMPILING_EMULATOR@
ar=@CMAKE_AR@
pkg_config=@PKG_CONFIG_EXECUTABLE@

[compilers]
# The name of the compiler should be the language name as defined with IDE_TOOLCHAIN_LANGUAGE_ keys
c=@CMAKE_C_COMPILER@
c++=@CMAKE_CXX_COMPILER@
vala=@CMAKE_VALA_COMPILER@
