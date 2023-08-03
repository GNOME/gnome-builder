#############################
Creating Your First Extension
#############################

Plugins consist of two things.
First, a meta-data file describing the extension which includes things like a name, the author, and where to find the extension.
Second, the code which can take the form of a shared library or python module.

Builder supports writing extensions in C, Vala, or Python.
We will be using Python for our examples in this tutorial because it is both succinct and easy to get started with.

First, we will look at our extension meta-data file.
The file should have the file-suffix of ".plugin" and its format is familiar.
It starts with a line containing "[Plugin]" indicating this is extension metadata.
Then it is followed by a series of "Key=Value" key-pairs.

We will often use the words "extension" and "plugin" interchangeably.

.. code-block:: ini

   # my_plugin.plugin
   [Plugin]
   Name=My Plugin
   Loader=python3
   Module=my_plugin
   Author=Angela Avery
   X-Builder-ABI=3.32
   X-Has-Resources=true

.. note:: ``X-Builder-ABI`` should be set to the version of Builder you are targeting (not including the micro release number). It will only be loaded if the Builder version matches.

Now we can create a simple plugin that will print "hello" when Builder starts and "goodbye" when Builder exits.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Ide

   class MyAppAddin(GObject.Object, Ide.ApplicationAddin):

       def do_load(self, application):
           print("hello")

       def do_unload(self, application):
           print("goodbye")

In the python file above, we define a new extension called ``MyAppAddin``.
It inherits from ``GObject.Object`` (which is our base object) and implements the interface ``Ide.ApplicationAddin``.
We won't get too much into objects and interfaces here, but the plugin manager uses this information to determine when and how to load our extension.

The ``Ide.ApplicationAddin`` requires that two methods are implemented.
The first is called ``do_load`` and is executed when the extension should load.
And the second is called ``do_unload`` and is executed when the plugin should cleanup after itself.
Each of the two functions take a parameter called ``application`` which is an ``Ide.Application`` instance.

Loading our Extension
=====================

Now place the two files in ``~/.local/share/gnome-builder/plugins`` as ``my_plugin.plugin`` and ``my_plugin.py``.
If we run Builder from the command line, we should see the output from our plugin!

.. code-block:: sh

   [angela@localhost ~] gnome-builder
   hello

Now if we close the window, we should see that our plugin was unloaded.

.. code-block:: sh

   [angela@localhost ~] gnome-builder
   hello
   goodbye

.. _embedding_resources:

Embedding Resources
===================

Sometimes plugins need to embed resources. Builder will automatically
load a file that matches the name ``$module_name.gresource`` if it
placed alongside the ``$module_name.plugin`` file.

.. note:: If you are writing an extension in C or Vala, simply embed GResources as normal.

.. code-block:: xml
   :caption: First we need to create a my-plugin.gresource.xml file describing our resources

   <?xml version="1.0" encoding="UTF-8"?>
   <gresources>
     <gresource prefix="/plugins/my-plugin">
       <file preprocess="xml-stripblanks" compressed="true">gtk/menus.ui</file>
     </gresource>
   </gresources>

Next, compile the resources using ``glib-compile-resources``.

.. code-block:: sh

   glib-compile-resources --generate my-plugin.gresource my-plugin.gresource.xml

Now you should have a file named ``my-plugin.gresource`` in the current directory.
Ship this file along with your ``my-plugin.plugin`` and Python module.

Next, continue on to learn about other interfaces you can implement in Builder to extend it's features!
