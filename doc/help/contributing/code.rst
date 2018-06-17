#################
Contributing Code
#################

Where to Contribute?
====================

Builder wants to become a powerful tool to enable GNOME developers to build great software.
To do this we need your help.

Do you have knowledge in a particular area of software development?
You can use that knowledge to help Builder expand it's area of expertise.

Generally, code contributions fall into one of two categories: `Application Plumbing`_ or `Plugins`_.

Application Plumbing
--------------------

If you like working on application plumbing, which is the infrastructure that makes implementing plugins simple, then you want to look at libide_.
Many of the core features of Builder are implemented here.
That includes the application window, plugin interfaces and core machinery of Builder.

Plugins
-------

Plugins are how we integrate features into Builder for a specific problem.
For example, the git_ plugin is the glue between the version control abstraction in libide_ and git.

There are many `existing plugins`_ already.
You might want to contribute to an existing one that does not yet serve your needs well.
Or maybe you want to :ref:`create a new plugin<creating_plugins>` that integrates a feature missing from Builder.

Running
-------

You might find yourself running Builder from Builder.
By default, that will activate and bring-forward your previous instance of Builder.
If you run Builder from the command-line with ``--standalone``, it will not communicate with another instance of Builder.

.. _libide: https://gitlab.gnome.org/GNOME/gnome-builder/tree/master/src/libide/
.. _git: https://gitlab.gnome.org/GNOME/gnome-builder/tree/master/src/plugins/git
.. _`existing plugins`: https://gitlab.gnome.org/GNOME/gnome-builder/tree/master/src/plugins

