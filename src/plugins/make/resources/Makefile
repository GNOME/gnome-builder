all: {{exec_name}}


{{if language == "c"}}
WARNINGS = -Wall
DEBUG = -ggdb -fno-omit-frame-pointer
OPTIMIZE = -O2

{{else if language == "c++"}}
WARNINGS = -Wall -Weffc++ -Wextra -Wsign-conversion -pedantic-errors
DEBUG = -ggdb -fno-omit-frame-pointer
OPTIMIZE = -O2
STANDARD = -std=c++2a
{{end}}

{{if language == "c"}}
{{exec_name}}: Makefile {{exec_name}}.c
	$(CC) -o $@ $(WARNINGS) $(DEBUG) $(OPTIMIZE) {{exec_name}}.c
{{else if language == "c++"}}
{{exec_name}}: Makefile {{exec_name}}.cpp
	$(CXX) -o $@ $(WARNINGS) $(DEBUG) $(OPTIMIZE) $(STANDARD) {{exec_name}}.cpp
{{end}}

clean:
	rm -f {{exec_name}}


# Builder will call this to install the application before running.
install:
	echo "Installing is not supported"

# Builder uses this target to run your application.
run:
	./{{exec_name}}

