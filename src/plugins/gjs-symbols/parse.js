try {
    const DATA_FD = 0;
    const Gio = imports.gi.Gio;
    const GLib = imports.gi.GLib;

    let input = Gio.UnixInputStream.new(DATA_FD, true);
    let reader = Gio.DataInputStream.new(input);
    let [data, len] = reader.read_upto("", -1, null);

    print(JSON.stringify(Reflect.parse(data, {source: ARGV[1], line: 0, target: "module"})));
} catch (e) {
    print(e);
    imports.system.exit(1);
}
