Extend HeaderBar and OmniBar
============================

You might want to add a button to the workspace header bar.
To do this, use an ``Ide.WorkspaceAddin`` and fetch the header bar using ``Ide.Workspace.get_headerbar()``.
You can attach your widget to either the left or the right side of the ``Ide.OmniBar`` in the center of the header bar.

We suggest using ``Gio.SimpleAction`` to attach an action to the workspace and then activating the action using the ``Gtk.Button:action-name`` property.

.. code-block:: python3
   :caption: Adding a button to the workspace header bar

   import gi

   from gi.repository import GObject, Ide

   class MyWorkspaceAddin(GObject.Object, Ide.WorkspaceAddin):
       """
       Add a new button to the header bar.
       """

       def do_load(self, workspace: Ide.Workspace):
           headerbar = workspace.get_headerbar()

           # Add button to top-center-left
           self.button = Gtk.Button(label='Click', action_name='win.hello', visible=True)
           headerbar.add_center_left(self.button)

           # Add button to left
           self.button = Gtk.Button(label='Click', action_name='win.hello', visible=True)
           headerbar.add_primary(self.button)

           # Add button to right
           self.button = Gtk.Button(label='Click', action_name='win.hello', visible=True)
           headerbar.add_secondary(self.button)

       def do_unload(self, workspace: Ide.Workspace):
           # remove the button we added
           self.button.destroy()
           self.button = None

   class MyOmniBarAddin(GObject.Object, Ide.OmniBarAddin):
       """
       Extend the omnibar by adding a button inside the bar.
       """

       def do_load(self, omni_bar: Ide.OmniBar):
           self.button = Gtk.Button(visible=True, label='Hi')
           omni_bar.add_status_icon(self.button, 0)

       def do_unload(self, omni_bar: Ide.OmniBar):
           self.button.destroy()
           self.button = None
