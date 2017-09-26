#################################
Creating and Performing Transfers
#################################

Builder provides a transfers button similar to a web browser.
It should feel familiar to users of Nautilus (Files) and Epiphany (Web).

.. image:: ../figures/transfers.png
   :width: 527 px
   :align: center

To integrate with the transfers button, you need to create a subclass of
``Ide.Transfer`` and add it to the ``Ide.TransferManager``.

Builder provides a convenience transfer class called ``Ide.PkconTransfer``
which can be used to install packages on the host machine.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Gio
   from gi.repository import Ide

   class MyTransfer(Ide.Transfer):
       def do_execute_async(self, cancellable, callback, data):
           task = Gio.Task.new(self, cancellable, callback)

           # ... perform transfer asynchronously, and update
           # progress from the main thread occasionally, using
           # self.set_progress(0..1)

           self.set_status('Downloading...')
           self.set_progress(0.1)
           self.set_icon_name('gtk-missing')

           # When finished, complete the task
           task.return_boolean(True)

           # Unless there was a failure, return the error
           task.return_error(GLib.Error(my_exception))

       def do_execute_finish(self, task):
           return task.propagate_boolean()


   # From your other plugin code, get the transfer manager and
   # queue the transfer.

   app = Gio.Application.get_default()
   xfermgr = app.get_transfer_manager()

   xfer = MyTransfer(title='Downloading Foo')
   xfermgr.execute_async(xfer, None, None, None)

