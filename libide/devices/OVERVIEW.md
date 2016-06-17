# Devices

Devices are plumbed into builder so that we can connect external devices
and push/pull applications from them, remote debug, etc. However, not
much of that exists yet.

## IdeDevice

Represents a device, such as the local computer or a tablet.

## IdeDeviceManager

Manages all the devices loaded in the context.

## IdeDeviceProvider

Plugin interface that creates devices and adds them to the device manager.

