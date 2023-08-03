############################
Integrating Language Servers
############################

In order to integrate a language server with **GNOME Builder** you have to create an ``Ide.SubprocessLauncher``
to startup the language server.

This subprocess should be restarted if it breaks therefore we have to wrap this
``Ide.SubprocessLauncher`` in a ``Ide.SubprocessSupervisor`` to monitor the
external process. After the subprocess is started we connect ``stdin`` and ``stdout``
to our ``Ide.LspClient`` for dispatching of messages between client and server.

.. code-block:: python3

   class LSPService(Ide.Object):
      _has_started = False
      _client = None

      @GObject.Property(type=Ide.LspClient)
      def client(self):
         return self._client

      @client.setter
      def client(self, value):
         self._client = value
         self.notify('client')

      def start(self):
         if not self._has_started:
            self._has_started = True
            launcher = Ide.SubprocessLauncher()
            launcher.set_flags(Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDOUT_PIPE)
            launcher.push_argv('my_language_server_executable')

            supervisor = Ide.SubprocessSupervisor()
            supervisor.connect('spawned', self.lsp_spawned)
            supervisor.set_launcher(launcher)
            supervisor.start()

      def lsp_spawned(self, supervisor, subprocess):
         stdin = subprocess.get_stdin_pipe()
         stdout = subprocess.get_stdout_pipe()
         io_stream = Gio.SimpleIOStream.new(stdout, stdin)

         client = Ide.LspClient.new(io_stream)
         self.append(client)
         client.add_language('my_language')
         client.start()
         self.client(client)


As a language server handles several parts of an IDE we have to create according
extensions for code completion, diagnostics or hover content. As we want to make
sure that the corresponding service is only started once we use the builtin object
system (``Ide.Context.ensure_child_type(type)``.

.. code-block:: python3

   class MyLspCompletionProvider(Ide.LspCompletionProvider, Ide.CompletionProvider):

      def do_load(self, context):
         service = context.ensure_child_typed(LSPService)
         service.start()
         service.bind_property('client', self, 'client', GObject.BindingFlags.SYNC_CREATE)

      def do_get_priority(self, context):
         return 0

.. code-block:: python3

   class MyLspDiagnosticProvider(Ide.LspDiagnosticProvider, Ide.DiagnosticProvider):

      def do_load(self):
         context = self.get_context()
         service = context.ensure_child_typed(LSPService)
         service.start()
         service.bind_property('client', self, 'client', GObject.BindingFlags.SYNC_CREATE)
