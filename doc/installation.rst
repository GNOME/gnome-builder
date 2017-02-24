
.. _Installation:
.. _Flatpak: https://flatpak.org
.. _Stable: https://git.gnome.org/browse/gnome-apps-nightly/plain/gnome-builder.flatpakref?h=gnome-3-22
.. _Nightly: https://git.gnome.org/browse/gnome-apps-nightly/plain/gnome-builder.flatpakref
.. _Software: https://wiki.gnome.org/Apps/Software
.. _GNOME: https://gnome.org/
.. _JHBuild: https://wiki.gnome.org/Newcomers/BuildGnome
.. _`Newcomers Tutorial`: https://wiki.gnome.org/Newcomers
.. _`filing a bug`: https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-builder

############
Installation
############


The preferred installation method for Builder is `via Flatpak`_.
This provides a bandwidth efficient and safe to use installation method that can be easily kept up to date.
It is also the engine behind Builder's powerful SDK!


via Flatpak
-----------

If you have a recent Linux distribution, such as Fedora 25, simply download our Stable_ flatpak and click **Install** when Software_ opens.
If Software_ does not automatically open, try opening the Stable_ flatpakref from your file browser.

If you want to track Builder development, you might want our Nightly_ channel instead of Stable_.

Command Line
^^^^^^^^^^^^

You can also use the command line to install Builder.

**Stable**

.. code-block:: sh

   $ flatpak install --from https://git.gnome.org/browse/gnome-apps-nightly/plain/gnome-builder.flatpakref?h=gnome-3-22
   $ flatpak run org.gnome.Builder

**Nightly**

.. code-block:: sh

   $ flatpak install --from https://git.gnome.org/browse/gnome-apps-nightly/plain/gnome-builder.flatpakref
   $ flatpak run org.gnome.Builder

.. note:: Nightly builds are built with tracing enabled. The tracing is fairly lightweight, but it includes a great deal of more debugging information.


via JHBuild
-----------

If you plan on contributing to the GNOME desktop and application suite, you may want to install Builder via JHBuild_.
See the `Newcomers Tutorial`_ for more information on joining the community and installing JHBuild_.

We are aggresively moving towards using Flatpak for contributing to Builder, but we aren't quite there yet.

Command Line
^^^^^^^^^^^^

.. code-block:: sh

   $ git clone git://git.gnome.org/jhbuild.git
   $ cd jhbuild
   $ ./autogen.sh --simple-install
   $ make
   $ make install
   $ jhbuild sysdeps --install gnome-builder
   $ jhbuild build gnome-builder
   $ jhbuild run gnome-builder

.. warning:: Do not install JHBuild via your Linux distribution's package manager. It will be out of date.


via Release Tarball
-------------------

We do not recommend installing from release tarballs unless you are a Linux distribution.
Builder has a complex set of dependencies which heavily target the current release of GNOME.
Keeping up with these requires updating much of the GNOME desktop.

You probably want to install via Flatpak, which does not have this restriction.


Troubleshooting
---------------

If you are having troubles running Builder, we suggest running with verbose output as it will log more information about the running system.
The ``gnome-builder`` program can take multiple arguments of ``-v`` to increase verbosity.
For example, if running from ``flatpak``, you can increase the logging verbosity like:

.. code-block:: sh

    $ flatpak run org.gnome.Builder -vvvv

If you're running from a system installed package of Builder, the binary name is ``gnome-builder``.

.. code-block:: sh

   $ gnome-builder -vvvv

If youre issue persists, please consider `filing a bug`_.
