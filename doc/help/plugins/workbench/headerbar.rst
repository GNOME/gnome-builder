Adding Widgets to the Header Bar
================================

You might want to add a button to the workbench header bar.
To do this, use an ``Ide.WorkbenchAddin`` and fetch the header bar using ``Ide.Workbench.get_headerbar()``.
You can attach your widget to either the left or the right side of the ``Ide.OmniBar`` in the center of the header bar.
Additionally, by specifying a ``Gtk.PackType``, you can align the button within the left or right of the header bar.

We suggest using ``Gio.SimpleAction`` to attach an action to the workbench and then activating the action using the ``Gtk.Button:action-name`` property.

.. code-block:: python3
   :caption: Adding a button to the workbench header bar

   import gi

   from gi.repository import GObject
   from gi.repository import Ide

   class MyWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):

       def do_load(self, workbench):
           headerbar = workbench.get_headerbar()

           # Add button to top-center-left
           self.button = Gtk.Button(label='Click', action_name='win.hello', visible=True)
           headerbar.insert_left(self.button, Gtk.PackType.PACK_END, 0)

       def do_unload(self, workbench):
           # remove the button we added
           self.button.destroy()
           self.button = None

