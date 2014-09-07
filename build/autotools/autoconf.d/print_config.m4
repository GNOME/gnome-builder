m4_include([build/autotools/autoconf.d/pre-config.m4])
PROJECT_NAME=project_name
echo "
${PROJECT_NAME} configured with the following options:
"
m4_include([build/autotools/autoconf.d/post-config.m4])
echo "
Run \"make\" to build ${PROJECT_NAME}.
"
