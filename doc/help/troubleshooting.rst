###############
Troubleshooting
###############

If you are having trouble with Builder you can help us help you by trying to do some basic troubleshooting.
Here are some steps you can go through to try to discover what is going wrong.

Verbose Output
--------------

You can increase the log verbosity of Builder by adding up to four ``-v`` when launching from the command line.

.. code-block:: sh

   # If running from flatpak
   flatpak run org.gnome.Builder -vvvv

   # If using distribution packages
   gnome-builder -vvvv

Support Log
-----------

Builder has support to generate a support log which can provide us with details.
From the application menu, select “Generate Support Log”.
It will place a log file in your home directory.

Test Builder Nightly
--------------------

If you are running the stable branch or an older distribution package, please consider trying our Nightly release to see if the bug has already been fixed.
Doing this before reporting bugs helps reduce the amount of bug traffic we need to look at.
We'll usually ask you to try Nightly anyway before continuing the troubleshooting process.

See :ref:`installing from Flatpak<via_flatpak>` for installation notes.

File a Bug
----------

We can help you troubleshoot!
File a bug if you're stuck and we can help you help us.

Report a bug using the `Builder's Gitlab project`_.

.. _`Builder's Gitlab project`: https://gitlab.gnome.org/GNOME/gnome-builder/-/issues
