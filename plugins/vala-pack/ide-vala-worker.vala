namespace Ide {
	[DBus (name="org.gnome.builder.plugin.vala-pack")] // dbus interface name
	public class ValaDBusService : Object {

	}

	[DBus (name="org.gnome.builder.plugin.vala-pack")] // dbus interface name
	public interface ValaDBusClient : Object {

	}

	public class ValaWorker : Object, Ide.Worker {
		public DBusProxy create_proxy (DBusConnection conn) throws IOError {
			ValaDBusClient client = conn.get_proxy_sync (
				null, // bus name
				"/",
				DBusProxyFlags.DO_NOT_LOAD_PROPERTIES |
				DBusProxyFlags.DO_NOT_CONNECT_SIGNALS |
				DBusProxyFlags.DO_NOT_AUTO_START_AT_CONSTRUCTION,
				null);
			return client as DBusProxy;
		}

		public void register_service (DBusConnection conn) {
			try {
				conn.register_object ("/", new ValaDBusService ());
			} catch (IOError err) {
				critical ("Could not register ValaDBusService object: %s",
					err.message);
			}
		}
	}
}
