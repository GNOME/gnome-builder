Registering Perspectives
========================

Everything in Builder below the header bar is implemented as a "Perspective".
For example, the editor is a perspective and so is the preferences interface.

You may want to create a perspective if you require the users full attention
and other editor components may be distracting.

.. note:: We generally suggest looking for alternatives to creating a perspective as it
          can be cumbersome for the user to switch contexts.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Ide
   from gi.repository import Gtk

   class MyPerspective(Gtk.Bin, Ide.Perspective):
       def __init__(self, *args, **kwargs):
           super().__init__(*args, **kwargs)

           self.add(Gtk.Label(label='My Perspective', visible=True))

       def do_get_icon_name(self):
           return 'gtk-missing'

       def do_get_title(self):
           return 'My Perspective'

       def do_get_accelerator(self):
           return '<Control><Shift><Alt>Z'

   class MyWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):
       perspective = None

       def do_load(self, workbench):
           self.perspective = MyPerspective(visible=True)
           workbench.add_perspective(self.perspective)

       def do_unload(self, workbench):
           self.perspective.destroy()
           self.perspective = None

