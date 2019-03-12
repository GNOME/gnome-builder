Basics
======

The basic mechanics of extending the workbench requires first creating an ``Ide.WorkbenchAddin``.
Your subclass will created for each instance of the ``Ide.Workbench``.
This conveniently allows you to track the state needed for your plugin for each workbench.

.. code-block:: python3
   :caption: A Basic WorkbenchAddin to demonstrate scaffolding
   :name: basic-workbench-addin-py

   import gi

   from gi.repository import GObject
   from gi.repository import Ide

   class BasicWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):

       def do_load(self, workbench: Ide.Workbench):
           pass

       def do_unload(self, workbench: Ide.Workbench):
           pass

       def do_workspace_added(self, workspace: Ide.Workspace):
           pass

       def do_workspace_removed(self, workspace: Ide.Workspace):
           pass

   class BasicWorkspaceAddin(GObject.Object, Ide.WorkspaceAddin):

       def do_load(self, workspace: Ide.Workspace):
           pass

       def do_unload(self, workspace: Ide.Workspace):
           pass

       def do_surface_set(self, surface: Ide.Surface):
           pass

You will notice that at the top we import the packages we'll be using.
Here we use the ``GObject`` and ``Ide`` packages from GObject Introspection.

We then create a class which inherits from ``GObject.Object`` and implements the ``Ide.WorkbenchAddin`` interface.
The ``Ide.WorkbenchAddin`` interface has two virtual methods to override, ``Ide.WorkbenchAddin.load()`` and ``Ide.WorkbenchAddin.unload()``.

.. note:: PyGObject uses ``do_`` prefix to indicate we are overriding a virtual method.

The ``load`` virtual method is called to allow the plugin to initialize itself.
This method is called when the workbench is setup or your plugin is loaded.

When the ``unload`` virtual method is called the plugin should clean up after itself to leave Builder and the workbench in a consistent state.
This method is called when the workbench is destroyed or your plugin is unloaded.

To simplify tracking workspace surface changes, you can use ``Ide.WorkspaceAddin`` as the second class implements.
This plugin instance will be created for each workspace window.

.. note:: You can limit the types of workspaces that the ``Ide.WorkspaceAddin`` will be instantiated for using ``X-Workspace-Kind=primary;editor;greeter;`` in the ``.plugin`` file.
