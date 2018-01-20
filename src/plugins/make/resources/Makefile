all: {{exec_name}}


WARNINGS = -Wall
DEBUG = -ggdb -fno-omit-frame-pointer
OPTIMIZE = -O2

{{exec_name}}: Makefile {{exec_name}}.c
	$(CC) -o $@ $(WARNINGS) $(DEBUG) $(OPTIMIZE) {{exec_name}}.c

clean:
	rm -f {{exec_name}}


# Builder will call this to install the application before running.
install:
	echo "Installing is not supported"

# Builder uses this target to run your application.
run:
	./{{exec_name}}


