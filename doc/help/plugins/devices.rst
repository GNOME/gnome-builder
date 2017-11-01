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

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Ide

   class MyDevice(Ide.Device):
       def do_get_system_type(self):
           return 'x86_64-linux-gnu'

   class MyDeviceProvider(Ide.Object, Ide.DeviceProvider):
       settled = GObject.Property(type=bool)

       def __init__(self, *args, **kwargs):
           super().__init__(*args, **kwargs)

           self.devices = []

           # Start loading the devices, and then emit device-added
           # when it is added.
           device = MyDevice(id='my-device', display_name='My Device')
           self.devices.add(device)

           # Since we are in __init__ here, this isn't really necesssary
           # because it's impossible to connect and receive the signal.
           # However, if you do some background work, you'll need to
           # do this to notify the device manager of the new device.
           self.emit_device_added(device)

           # Mark us as "settled" which lets the device manager know
           # that we've completed our initial scan of devices.
           self.settled = True
           self.notify('settled')

       def do_get_devices(self):
           return self.devices

