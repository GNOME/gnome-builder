################
Session Tracking
################

Some plugins may want to save state when the user closes Builder.
The `Ide.SessionAddin` allows for saving and restoring state when a project is closed or re-opened.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Gio
   from gi.repository import Ide

   class MySessionAddin(Ide.Object, Ide.SessionAddin):

       def do_save_async(self, cancellable, callback, data):
           # Create our async task
           task = Ide.Task.new(sel, cancellable, callback)

           # State is saved as a variant
           task.result = GLib.Variant.new_int(123)

           # Now complete task
           task.return_boolean(True)

       def do_save_finish(self, task):
           if task.propagate_boolean():
               return task.result

       def do_restore_async(self, state, cancellable, callback, data):
           # Create our async task
           task = Ide.Task.new(sel, cancellable, callback)

           # state is a GLib.Variant matching what we saved

           # Now complete task
           task.return_boolean(True)

       def do_restore_finish(self, task):
           return task.propagate_boolean()
