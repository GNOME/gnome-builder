.. _`Builder Gitlab Issues`: https://gitlab.gnome.org/GNOME/gnome-builder/issues/new
.. _`Builder Bugzilla`: https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-builder&component=docs
.. _`git workflow`: https://wiki.gnome.org/Newcomers/SubmitPatch


#####################
Writing Documentation
#####################

We are using sphinix to write our new documentation.

In ``conf.py`` you'll see that we use the theme from readthedocs.io.  That
means you need to install that theme as well as sphinx to build the
documetnation.

.. code-block:: bash
   :caption: Install dependencies for building documentation (Fedora 25)
   :name: install-sphinx-deps-fedora

   sudo dnf install python3-sphinx python3-sphinx_rtd_theme


.. code-block:: bash
   :caption: Now build the documentation with sphinx
   :name: sphinx-build

   [user@host gnome-builder/]$ cd doc
   [user@host doc/]$ sphinx-build . _build
   [user@host doc/]$ xdg-open _build/index.html

The first command builds the documentation.
Pay attention to warnings which will be shown in red.
Some of them may be useful to help you track down issues quickly.

To open the documentation with your web browser, use ``xdg-open _build/index.html``.

Submitting Patches
==================

We will accept patches for documentation no matter how you get them to us.
However, you will save us a lot of time if you can:

 * Create a patch with git.
 * Create a new bug on `Builder Gitlab Issues`_ and attach the patch.

Creating a Patch
================

First off, if you have not configured git to include your full name and email, type the following in a terminal:

.. code-block:: bash

   $ git config --global user.name 'My Full Name'
   $ git config --global user.email 'example@example.com'

After you have modified the documentation to your liking, prepare the files to be committed to git.
The add a short commit message and commit the files.

.. code-block:: bash

   [user@host doc/]$ git add path/to/file.rst
   [user@host doc/]$ git commit -m 'doc: update example documentation'

Now we can export the patch to be uploaded to `Builder Gitlab Issues`_

.. code-block:: bash

   [user@host doc/]$ git format-patch HEAD^

At this point you'll see a file similar to ``0001-doc-update-example-documentation.patch`` in the current directory.
We want to upload this patch to `Builder Gitlab Issues`_.

Submitting a Patch
==================

Now that we have our patch file, we need to create a new bug.
Head over to `Builder Gitlab Issues`_ and fill out the bug details.

Just give a bit of information about what you documented, and then find the "Add an Attachment" section near the bottom.
Upload the patch file you exported with ``git format-patch HEAD^`` above.

Click "Submit Bug" and we'll take care of the rest!

GNOME git Best Practices
========================

To learn more about using git with GNOME, including how to set up git, submitting patches,
and good commit messages, visit the `git workflow`_ GNOME wiki page.
