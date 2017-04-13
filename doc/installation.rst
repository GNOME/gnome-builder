
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

If you have a recent Linux distribution, such as Fedora 25, simply download the Stable_ Flatpak and click **Install** when Software_ opens.
If Software_ does not automatically open, try opening the Stable_ flatpakref by double clicking it in your file browser.

If you want to track Builder development, you might want the Nightly_ channel instead of Stable_.

.. note:: To build flatpak-based applications, ensure that the ``flatpak-builder`` program is installed. On Fedora, this is the ``flatpak-builder`` package.

Command Line
^^^^^^^^^^^^

You can also use the command line to install Builder:

**Stable**

.. code-block:: sh

   $ flatpak install --user --from https://git.gnome.org/browse/gnome-apps-nightly/plain/gnome-builder.flatpakref?h=gnome-3-22
   $ flatpak run org.gnome.Builder

**Nightly**

.. code-block:: sh

   $ flatpak install --user --from https://git.gnome.org/browse/gnome-apps-nightly/plain/gnome-builder.flatpakref
   $ flatpak run org.gnome.Builder

.. note:: Nightly builds are built with tracing enabled. The tracing is fairly lightweight, but it includes a great deal of more debugging information.

Local Flatpak Builds
--------------------

You can also build Builder as a flatpak yourself to test local changes. First, make a repo for your local builds:

.. code-block:: sh

   $ mkdir ~/my-flatpak-builds
   $ flatpak remote-add --user --no-gpg-verify my-flatpak-builds ~/my-flatpak-builds

Now, in Builder's source directory, use ``flatpak-builder`` to build a Builder flatpak and install it

.. code-block:: sh

   $ git clone https://git.gnome.org/browse/gnome-builder/
   $ cd gnome-builder
   $ mkdir app
   $ flatpak-builder --ccache --repo=$HOME/my-flatpak-builds app org.gnome.Builder.json
   $ flatpak install --user my-flatpak-builds org.gnome.Builder

.. note:: After following these steps once you can omit adding the remote or creating the app directory. You'll also need to add the ``--force-clean`` option to flatpak-builder and use ``flatpak update`` rather than ``flatpak install``.

.. _via-jhbuild:

via JHBuild
-----------

If you plan on contributing to the GNOME desktop and application suite, you may want to install Builder via JHBuild_.
See the `Newcomers Tutorial`_ for more information on joining the community and installing JHBuild_.

We are aggressively moving towards using Flatpak for contributing to Builder, but we aren't quite there yet.

Command Line
^^^^^^^^^^^^

.. note:: Please review the GNOME `Newcomers Tutorial`_ on how to build a GNOME application before proceeding.

.. code-block:: sh

   # Make sure you have the following packages installed before starting

   # On Fedora
   $ sudo dnf install clang-devel llvm-devel libssh2-devel

   # On Ubuntu
   $ sudo apt-get install clang-3.9 libclang-3.9-dev llvm-3.9-dev libssh2-1-dev


.. code-block:: sh

   $ git clone git://git.gnome.org/jhbuild.git
   $ cd jhbuild
   $ ./autogen.sh --simple-install
   $ make
   $ make install
   $ jhbuild sysdeps --install gnome-builder
   $ jhbuild build gnome-builder
   $ jhbuild run gnome-builder

.. warning:: While it may be tempting to install jhbuild using your Linux distribution's package manager, it will lack an updated description of the GNOME modules and is therefore insufficient. Always install jhbuild from git.


via Release Tarball
-------------------

We do not recommend installing from release tarballs unless you are a Linux distribution.
Builder has a complex set of dependencies which heavily target the current release of GNOME.
Keeping up with these requires updating much of the GNOME desktop.

Please install via Flatpak, which does not have this restriction.


Troubleshooting
---------------

If you are having trouble running Builder, start Builder with verbose output. 
This will log more information about the running system.
The ``gnome-builder`` program can take multiple arguments of ``-v`` to increase verbosity.
For example, if running from ``flatpak``:

.. code-block:: sh

    $ flatpak run org.gnome.Builder -vvvv

If you're running from a system installed package of Builder, the binary name is ``gnome-builder``.

.. code-block:: sh

   $ gnome-builder -vvvv

If your issue persists, please consider `filing a bug`_.
