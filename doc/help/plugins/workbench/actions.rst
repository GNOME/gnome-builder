Registering Workspace Actions
=============================

Using ``Gio.Action`` is a convenient way to attach actions to the workspace that contain state.
For example, maybe for use by a button that should be insensitive when it cannot be used.
Additionally, actions registered on the workspace can be activated using the command bar plugin.

.. code-block:: python3
   :caption: Registering an action on the workspace

   import gi

   from gi.repository import GObject
   from gi.repository import Gio
   from gi.repository import Ide

   class MyWorkspaceAddin(GObject.Object, Ide.WorkspaceAddin):

       def do_load(self, workspace):
           action = Gio.SimpleAction.new('hello', None)
           action.connect('activate', self.hello_activate)
           workspace.add_action(action)

           # If you have a lot of actions to add, you might
           # consider creating an action group.
           group = Gio.SimpleActionGroup.new()
           group.add_action(action)
           workspace.insert_action_group('my-actions', group)

       def do_unload(self, workspace):
           workspace.remove_action('hello')

           # And if you used an action group
           workspace.insert_action_group('my-actions', None)

       def hello_activate(self, action, param):
           print('Hello activated!')

This adds a new action named ``hello`` to the workspace.
It can be connected to widgets by using the ``win.hello`` action-name.
Additionally, you can call the action with ``hello`` from the command bar.

To toggle whether or not the action can be activated, set the ``Gio.SimpleAction:enabled`` property.
