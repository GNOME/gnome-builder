Registering Panels
==================


.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Ide

   class MyWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):

       def do_load(self, workbench):
           pass

       def do_unload(self, workbench):
           pass
