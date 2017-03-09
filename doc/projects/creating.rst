###############################
Creating and Importing Projects
###############################

Builder supports creating new projects as well as importing existing projects.
When importing a project, you can either open it from your local computer or clone from a remote git repository.

Creating a new Project
======================

To create a new project, select "New" from the project greeter.
You will be shown the project creation guide.

.. image:: ../figures/newproject.png
   :align: center

Give your project a meaningful name, as this is not easily changeable later.
It should not have spaces in the name.
If you need multiple words, consider using "-" to separate the words.

Choose the language you would like to use for the project.
This will affect what templates are available.

Choosing a license helps promote sharing of your application.
Builder is **GPLv3 or newer** and is our suggestion for writing new applications for GNOME.

If you do not want git-based version control, turn off the switch to disable git support.

Lastly, select a suitable template for your application.
Some patterns are available to speed up the bootstrapping of your project.


Cloning an Existing Project
===========================

To clone an existing project, you will need the URL of your **git repository**.
For example, to clone the Builder project, you could specify ``git://git.gnome.org/gnome-builder.git``.

.. image:: ../figures/clone.png
   :align: center

After entering the URL, click "Clone" in the upper right corner of the window and wait for the operation to complete.
Once the project has been cloned, you will be shown the workbench window.

.. note:: If the remote repository requires authorization you will be displayed with a dialog to provide credentials.
