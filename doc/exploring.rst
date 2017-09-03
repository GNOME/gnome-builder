#######################
Exploring the Interface
#######################

The following sections will help you get to know Builder.

 * `Project Greeter`_
 * `Workbench Window`_
 * `Header Bar`_
 * `Switching Perspectives`_
 * `Showing and Hiding Panels`_
 * `Build your Project`_
 * `Preferences`_
 * `Command Bar`_

Project Greeter
===============

When you start Builder, you will be asked to select a project to be opened:

.. image:: figures/greeter.png
   :width: 555 px
   :align: center

The window displays projects that were discovered on your system.
By default, the ``~/Projects`` directory will be scanned for projects when Builder starts.
Projects you have previously opened will be shown at the top.

Selecting a project row opens the project or pressing "Enter" will open the last project that was open.
You can also start typing to search the projects followed by "Enter" to open.

If you'd like to remove a previously opened project from the list, activate *Selection mode*.  
Press the "Select" button in the top right corner to the left of the close application button and
then select the row you would like to remove. 
Select the row(s) you'd like to remove and then click "Remove" in the lower left corner of the window.

Workbench Window
================

The application window containing your project is called the "**Workbench Window**".
The Workbench is split up into two main areas.
At the top is the `Header Bar`_ and below is the current "**Perspective**".

Builder has many perspectives, including the Editor, Build Preferences, Application Preferences, and the Profiler.

Header Bar
==========

The header bar is shown below.
This contains a button in the top left for `Switching Perspectives`_.
In the center is the "OmniBar" which can be used to `Build your Project`_.

.. image:: figures/workbench.png
   :align: center

To the right of the OmniBar is the *Run* button.
Clicking the arrow next to *Run* allows you to change how Builder will run your application.
You can run normally, with a debugger, profiler, or event with Valgrind.

On the right is the search box.
Type a few characters from the file you would like to open and it will fuzzy search your project tree.
Use "Enter" to complete the request and open the file.

To the right of the search box is the workbench menu.
You can find more options here such as `Showing and Hiding Panels`_.

Switching Perspectives
======================

To switch perspectives, click the perspective selector button in the top left of the workbench window.
Perspectives that support a keyboard accelerator will display the appropriate accelerator next to name of the perspective.

.. image:: figures/perspectives.png
   :width: 295 px
   :align: center

Select the row to change perspectives.

Showing and Hiding Panels
=========================

Sometimes panels get in the way of focusing on code.
You can move them out of the way using the buttons in the top left of the workbench window.

.. image:: figures/panels.png
   :width: 133 px
   :align: center

Additionally, you can use the "left-visible" or "bottom-visible" commands from the `Command Bar`_ to toggle their visibility.

Build your Project
==================

To build your project, use the OmniBar in the center of the header bar.
To the right of the OmniBar is a button for starting a build as shown in the image below.

.. image:: figures/omnibar.png
   :width: 708 px
   :align: center

You can also use the "build", "rebuild", "install", or "clean" commands from the command bar.

While the project is building, the build button will change to a cancel button.
Clicking the cancel button will abort the current build.

.. image:: figures/building.png
   :width: 623 px
   :align: center


Preferences
===========

The preferences perspective allows you to change settings for Builder and its plugins.
You can search for preferences using the keyword search in the top left of the preferences perspective.

.. image:: figures/preferences.png
   :align: center


Command Bar
===========

The command bar provides a command-line-interface into Builder.
You can type various actions to activate them.
If Vim-mode is enabled, you can also activate some Vim-inspired commands here.

The command bar includes tab completion as shown below.

.. image:: figures/commandbar.png
   :width: 1113 px
   :align: center

