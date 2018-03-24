####################
Use a Custom Sysroot
####################

Builder provides support for custom sysroots.
A sysroot is a system install in a directory that you are not currently using on your system.
You can configure a new sysroot in `Preferences -> SDKs` to allow Builder to use it.

When creating a new sysroot, you'll be asked for the following information.

- The name of your choice describing this sysroot.
- The architecture targeted by the sysroot such as `x86_64`, `aarch64`, `i386` or `arm`.
- The absolute path to the sysroot on your system.
- An optional `PKG_CONFIG_PATH` to use for the sysroot.

.. note::
   By default pkg-config will search in `/lib/pkgconfig` and `/usr/lib/pkgconfig` but some sysroots install them in different locations.
   Debian-based use paths such as `/usr/lib/x86_64-linux-gnu/pkgconfig`.
   Other systems may use paths such as `/usr/lib64/pkgconfig`.

The configuration will be stored in ~/.config/gnome-builder/sysroot/general.conf using a simple key-value format:

.. code-block:: cfg

   [Sysroot 0]
   Name=My Sysroot 😎
   Arch=x86_64
   Path=/path/to/my_sysroot
   PkgConfigPath=/path/to/my_sysroot/usr/lib/x86_64-linux-gnu/pkgconfig


Creating a Sysroot with Fedora
------------------------------

On Fedora, you can use `dnf` to create a new sysroot.

.. code-block:: sh

   # Create a new sysroot using the host system architecture for
   # Fedora 28 with gcc, binutils, and make installed. You can add
   # more packages as needed.
   sudo dnf install \
       --releasever=28 \
       --installroot=/opt/fedora-28/ \
       --repo=fedora -y \
       systemd passwd dnf fedora-release gcc binutils make
