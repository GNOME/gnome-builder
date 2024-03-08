.. _`merge request`: https://welcome.gnome.org/app/Builder/#submitting-your-work
.. _`Gitlab`: https://gitlab.gnome.org/GNOME/gnome-builder



#####################
Writing Documentation
#####################

We are using Sphinx to write our new documentation.

In ``conf.py`` you'll see that we use the theme from readthedocs.io. That 
means you need to install that theme as well as Sphinx to build the
documentation.

.. code-block:: bash
   :caption: Install dependencies for building documentation (Ubuntu)
   :name: install-sphinx-deps-ubuntu

   sudo apt install python3-sphinx python3-sphinx-rtd-theme

.. code-block:: bash
   :caption: Install dependencies for building documentation (Fedora 25)
   :name: install-sphinx-deps-fedora

   sudo dnf install python3-sphinx python3-sphinx_rtd_theme

.. code-block:: bash
   :caption: Now build the documentation with sphinx
   :name: sphinx-build

   [user@host gnome-builder/]$ cd doc/help/
   [user@host help/]$ sphinx-build . _build
   [user@host help/]$ xdg-open _build/index.html

The first command builds the documentation.

Pay attention to warnings which will be shown in red. Some of them may be 
useful to help you track down issues quickly.

To open the documentation with your web browser, use ``xdg-open _build/index.html``.

Submitting Patches
==================

We will accept patches for documentation no matter how you get them to us.
However, you will save us a lot of time if you can:

 * Create a fork on `Gitlab`_.
 * Create a `merge request`_ on `Gitlab`_.
