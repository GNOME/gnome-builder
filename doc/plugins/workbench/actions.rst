Registering Workbench Actions
=============================

Using ``Gio.Action`` is a convenient way to attach actions to the workbench that contain state.
For example, maybe for use by a button that should be insensitive when it cannot be used.
Additionally, actions registered on the workbench can be activated using the command bar plugin.

.. code-block:: python3
   :caption: Registering an action on the workbench

   import gi

   from gi.repository import GObject
   from gi.repository import Gio
   from gi.repository import Ide

   class MyWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):

       def do_load(self, workbench):
           action = Gio.SimpleAction.new('hello', None)
           action.connect('activate', self.hello_activate)
           workbench.add_action(action)

       def do_unload(self, workbench):
           workbench.remove_action('hello')

       def hello_activate(self, action, param):
           print('Hello activated!')

This adds a new action named ``hello`` to the workbench.
It can be connected to widgets by using the ``win.hello`` action-name.
Additionally, you can call the action with ``hello`` from the command bar.

To toggle whether or not the action can be activated. set the ``Gio.SimpleAction:enabled`` property.
