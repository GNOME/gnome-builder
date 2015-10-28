#!/bin/bash

set -o nounset
set -o errexit

contains() {
  for word in $1; do
    [[ "$word" = "$2" ]] && return 0
  done
  return 1
}

if [ "$#" -ne 1 ]; then
    echo "Missing def file"
fi

DEFS=$1

declare -A OPTS
declare -A CONFIGURE
declare -A SOURCES
declare -A BRANCHES
declare -A CUSTOM_PREPARE

source "$DEFS"

CHANGED=""

mkdir -p dl
mkdir -p cache
cd dl

STARTAT=""
BODY=""

for MODULE in $MODULES; do
    echo ========== Updating $MODULE ================
    URL=${SOURCES[$MODULE]}
    BRANCH=${BRANCHES[$MODULE]-master}
    BASENAME=`basename $URL`
    if [[ "$URL" =~ ^git: ]]; then
        if ! test -d $BASENAME.git; then
            git clone --mirror $URL --single-branch --branch $BRANCH
            CHANGED="$CHANGED $MODULE"
            cd $BASENAME.git
            REV=$(git rev-parse $BRANCH)
            cd ..
        else
            cd $BASENAME.git
            OLD_REV=""
            git rev-parse -q --verify refs/heads/$BRANCH && OLD_REV=`git rev-parse $BRANCH`
            git fetch origin $BRANCH
            REV=$(git rev-parse $BRANCH)
            if [ "x$OLD_REV" != "x$REV" ]; then
                CHANGED="$CHANGED $MODULE"
            fi
            cd ..
        fi
        BODY="$BODY$MODULE: $URL $REV"$'\n'
    elif [[ "$URL" =~ ^lp: ]]; then
        REPONAME=`echo $URL | sed s/.*://`
        if ! test -d $REPONAME.bzr; then
            bzr branch  $URL $REPONAME.bzr
            CHANGED="$CHANGED $MODULE"
            cd $REPONAME.bzr
            REV=$(bzr revno)
            cd ..
        else
            cd $REPONAME.bzr
            OLD_REV=$(bzr revno)
            bzr pull
            REV=$(bzr revno)
            if [ "x$OLD_REV" != "x$REV" ]; then
                CHANGED="$CHANGED $MODULE"
            fi
            cd ..
        fi
        BODY="$BODY$MODULE: $URL $REV"$'\n'
    elif [[ "$URL" =~ ^\. ]]; then
        if test -f "$BASENAME" && test -f "$URL"; then
            OLD_MD5=`md5sum $BASENAME`
            NEW_MD5=`md5sum $URL`
            if [ "x$OLD_MD5" != "x$NEW_MD5" ]; then
                cp "$URL" "$BASENAME"
                CHANGED="$CHANGED $MODULE"
            fi
        else
            cp "$URL" "$BASENAME"
            CHANGED="$CHANGED $MODULE"
        fi
        BODY="$BODY$MODULE: $URL"$'\n'
    else
        if ! test -f $BASENAME ; then
            curl -L -O $URL
            CHANGED="$CHANGED $MODULE"
        fi
        BODY="$BODY$MODULE: $URL"$'\n'
    fi

    # If anything changed in this module or before, blow away any caches
    if [ "x$CHANGED" != "x" ]; then
        rm -rf ../cache/cache-$APPID-$MODULE.tar
    elif test -f "../cache/cache-$APPID-$MODULE.tar"; then
        # No changes last and there is a cache for this module, start here
        echo Found cache $MODULE
        STARTAT=$MODULE
    fi
done

cd ..

if [ "x$CHANGED" == "x" -a "x${FORCE-}" == "x" ]; then
    echo "No changes - skipping rebuild"
    exit 0
fi

echo "Changed modules: $CHANGED"

rm -rf app
xdg-app build-init app $APPID $SDK $PLATFORM $SDK_VERSION

mkdir -p build

cd build
for MODULE in $MODULES; do
    if [ "x${STARTAT}" != "x" ]; then
        if [ "x${STARTAT}" == "x${MODULE}" ]; then
            echo ========== Using cache from $MODULE ================
            tar xf ../cache/cache-$APPID-$MODULE.tar -C ../app
            STARTAT=""
        else
            echo ========== Ignoring $MODULE ================
        fi;
        continue;
    fi

    echo ========== Building $MODULE ================

    OPT="${OPTS[$MODULE]-}"
    URL=${SOURCES[$MODULE]}
    BASENAME=`basename $URL`
    if [[ "$URL" =~ ^git: ]]; then
        DIR=$BASENAME
        rm -rf $DIR
        BRANCH=${BRANCHES[$MODULE]-master}
        git clone --shared --branch $BRANCH ../dl/$BASENAME.git
    elif [[ "$URL" =~ ^lp: ]]; then
        REPONAME=`echo $URL | sed s/.*://`
        DIR=$REPONAME
        rm -rf $REPONAME
        bzr branch --stacked ../dl/$REPONAME.bzr $REPONAME
    elif [[ "$URL" =~ .zip$ ]]; then
        DIR=${BASENAME%.zip}
        rm -rf $DIR
        unzip ../dl/$BASENAME

        OPT="$OPT noautogen"
    else
        DIR=${BASENAME%.t*}
        rm -rf $DIR
        tar xf ../dl/$BASENAME

        OPT="$OPT noautogen"
    fi

    CONFIGURE_ARGS="${CONFIGURE[$MODULE]-}"
    PREPARE="${CUSTOM_PREPARE[$MODULE]-}"

    cd "$DIR"
    if [ "x$PREPARE" != "x" ] ; then
        xdg-app build ../../app bash -c "$PREPARE"
    fi
    if ! contains "$OPT" nohelper ; then
        xdg-app build ../../app ../../build_helper.sh "$OPT" "--prefix=/app $CONFIGURE_ARGS"
    fi
    cd ..

    if contains "$CACHEPOINTS" "$MODULE" ; then
        tar cf ../cache/cache-$APPID-$MODULE.tar -C ../app files
    fi
done
cd ..

echo ========== Postprocessing ================

#xdg-app build app rm -rf /app/include
#xdg-app build app rm -rf /app/lib/pkgconfig
#xdg-app build app rm -rf /app/share/pkgconfig
#xdg-app build app rm -rf /app/share/aclocal
#xdg-app build app rm -rf /app/share/gtk-doc
#xdg-app build app rm -rf /app/share/man
#xdg-app build app rm -rf /app/share/info
#xdg-app build app rm -rf /app/share/devhelp
#xdg-app build app rm -rf /app/share/vala/vapi
xdg-app build app bash -c "find /app/lib -name *.a | xargs -r rm"
xdg-app build app bash -c "find /app -name *.la | xargs -r rm"
xdg-app build app bash -c "find /app -name *.pyo | xargs -r rm"
xdg-app build app bash -c "find /app -name *.pyc | xargs -r rm"
xdg-app build app glib-compile-schemas /app/share/glib-2.0/schemas
xdg-app build app gtk-update-icon-cache -f -t /app/share/icons/hicolor

xdg-app build app bash -c "find /app -type f | xargs file | grep ELF | grep 'not stripped' | cut -d: -f1 | xargs -r -n 1 strip"
if [ "x${DESKTOP_FILE-}" != x ]; then
    xdg-app build app mv /app/share/applications/${DESKTOP_FILE} /app/share/applications/${APPID}.desktop
fi
if [ "x${DESKTOP_NAME_SUFFIX-}" != x ]; then
    xdg-app build app sed -i "s/^Name\(\[.*\]\)\?=.*/&${DESKTOP_NAME_SUFFIX}/" /app/share/applications/${APPID}.desktop
fi
if [ "x${DESKTOP_NAME_PREFIX-}" != x ]; then
    xdg-app build app sed -i "s/^Name\(\[.*\]\)\?=/&${DESKTOP_NAME_PREFIX}/" /app/share/applications/${APPID}.desktop
fi
if [ "x${ICON_FILE-}" != x ]; then
    xdg-app build app bash -c "for i in \`find /app/share/icons -name ${ICON_FILE}\`; do mv \$i \`dirname \$i\`/${APPID}.png; done"
    xdg-app build app sed -i "s/Icon=.*/Icon=${APPID}/" /app/share/applications/${APPID}.desktop
fi
if [ "x${CLEANUP_FILES-}" != x ]; then
    xdg-app build app rm -rf ${CLEANUP_FILES-}
fi

TALK_NAMES=""
for talk in $TALK; do
    TALK_NAMES="$TALK_NAMES --talk-name=${talk}"
done

xdg-app build-finish --command=$COMMAND --share=ipc --socket=x11 --socket=pulseaudio --socket=session-bus --filesystem=host ${TALK_NAMES} ${FINISH_ARGS-} app
xdg-app build-export --subject="Nightly build of ${APPID}, `date`" --body="$BODY" ${EXPORT_ARGS-} repo app
