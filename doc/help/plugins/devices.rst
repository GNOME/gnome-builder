############################
Extending the Device Manager
############################

Builder provides an abstraction for external hardware devices. In fact, the
local development machine is a special device called the "local" device.

Devices are used in determining how to compile for the target architecture. In
the future, we expect to gain additional features such as deploying
applications to the device and debugging. However it is too soon to speculate
on that API.

You might want to provide support for an external device such as a embedded
board, tablet, or phone. To do this, use an ``Ide.DeviceProvider``.


.. code-block:: python3

   from gi.repository import Ide, Gio

   class FooDeviceProvider(Ide.DeviceProvider):

       def do_load_async(self, cancellable, callback, data):
           task = Gio.Task.new(self, cancellable, callback)
           task.devices = []

           # Do some device discovery, preferrably asynchronously if you have blocking I/O
           device = FooDevice(id='foo', display_name='My Headphones', icon_name='audio-headphones-symbolic')
           task.devices.append(device)

           task.return_boolean(True)

       def do_load_finish(self, task):
           if task.propagate_boolean():
               for device in task.devices:
                   self.emit_device_added(device)
               return True

   class FooDevice(Ide.Device):

       def do_prepare_configuration(self, config):
           # If you need to alter the Ide.Config in some way, this is where
           # you would do that.
           pass

       def do_get_info_async(self, cancellable, callback, data):
           # If you need to probe the device to get information about it
           # such as the host triplet, etc, this is where you would do
           # that. And again, preferrably asynchronously if blocking I/O
           # is required.
           task = Gio.Task.new(self, cancellable, callback)
           task.info = Ide.DeviceInfo.new()
           task.info.set_kind(Ide.DeviceKind.PHONE)
           task.info.set_host_triplet(Ide.Triplet.new('x86_64-linux-gnu'))
           task.return_boolean(True)

       def do_get_info_finish(self, task):
           if task.propagate_boolean():
               return task.info
