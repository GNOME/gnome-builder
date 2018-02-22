####################
Use a Custom Sysroot
####################

If you need to use a custom sysroot. You can configure a new target in `Preferences->SDKs`.

There are three fields to fill:

- Name: The name of your choice describing this sysroot.
- Sysroot path: The absolute path on your filesystem leading to the sysroot.
- Additional pkg-config path: (optional) A colon separated list of absolute path leading to pkg-config folders of the sysroot.

.. note:: By default pkg-config will search in /lib/pkgconfig and /usr/lib/pkgconfig but some systems are installing them in different directories (for instance a x86_64 Debian-based system will require /usr/lib/x86_64-linux-gnu/pkgconfig when a RPM-based system will use /usr/lib64/pkgconfig).

The configuration will be stored in ~/.config/gnome-builder/sysroot/general.conf using a simple key-value format:

.. code-block:: cfg

    [Sysroot 0]
    Name=My Sysroot 😎
    Path=/path/to/my_sysroot
    PkgConfigPath=/path/to/my_sysroot/usr/lib/x86_64-linux-gnu/pkgconfig
