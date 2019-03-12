Adding a Surface
================

Everything in Builder below the header bar is implemented as a "Surface".
For example, the editor is a surface and so is the "Build Preferences" interface.

You may want to create a surface if you require the users full attention and other editor components may be distracting.

.. note:: We generally suggest looking for alternatives to creating a surface as it can be cumbersome for the user to switch contexts.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject, Gtk, Ide

   class MySurface(Ide.Surface):
       def __init__(self, *args, **kwargs):
           super().__init__(*args, **kwargs)

           self.add(Gtk.Label(label='My Surface', visible=True))

           self.set_icon_name('gtk-missing')
           self.set_title('My Surface')

   class MyWorkspaceAddin(GObject.Object, Ide.WorkspaceAddin):
       surface = None

       def do_load(self, workspace: Ide.Workspace):
           if type(workspace) == Ide.PrimaryWorkspace:
               self.surface = MySurface(visible=True)
               workspace.add_surface(self.surface)

       def do_unload(self, workspace: Ide.Workspace):
           if self.surface is not None:
               self.surface.destroy()
               self.surface = None

