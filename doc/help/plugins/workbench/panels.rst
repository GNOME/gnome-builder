Adding Panels to the Workspace
==============================

You may want to write an extension that adds a panel which displays information to the user.
Builder provides API for this via the "Editor Surface".

At a high level, the design of the editor is broken into four parts.

 - The code editors in the center of the surface
 - The left panel, which contains the "sources" of actions
 - The bottom panel, which contains utilities
 - The right panel, which is transient and contextual to the operation at hand

The easiest way to add a panel is to register an ``Ide.EditorAddin`` which adds the panels in the ``do_load()`` function.
You'll be provided access to the editor surface with the ``editor`` variable.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject, Gtk, Dazzle, Ide

   class MyEditorAddin(GObject.Object, Ide.EditorAddin):

       def do_load(self, editor: Ide.EditorSurface):

           # Add a widget to the left panel (aka Sidebar)
           self.panel = Gtk.Label(visible=True, label='My Left Panel')
           left_panel = editor.get_sidebar()
           left_panel.add_section('my-section',
                                  'My Section Title',
                                  'my-section-icon-name',
                                  None, # Menu id if necessary
                                  None, # Menu icon name if necessary
                                  self.panel,
                                  100)  # Sort priority

           # To add a utility section
           self.bottom = Dazzle.DockWidget(title='My Bottom Panel', icon_name='gtk-missing', visible=True)
           self.bottom.add(Gtk.Label(label='Hello, Bottom Panel', visible=True))
           editor.get_utilities().add(self.bottom)

       def do_unload(self, editor: Ide.EditorSurface):

           # Remove our widgets
           self.panel.destroy()
           self.bottom.destroy()

           self.bottom = None
           self.panel = None


