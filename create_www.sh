#!/bin/bash

PROJECT_DIR="$(
    cd "$(dirname "$0")" >/dev/null 2>&1
    pwd -P
)"
cd $PROJECT_DIR
echo "Project path: $PROJECT_DIR"

APP_ROOT="${PROJECT_DIR}/www/weight-whiskers/"
APP_BUILD="${PROJECT_DIR}/www/weight-whiskers/build/"
APP_WWW="${PROJECT_DIR}/data/www"

# build react app
npm run build --prefix ${APP_ROOT}

# create filesystem image
rm -r ${APP_WWW}/*
mkdir -p ${APP_WWW}
cp -r ${APP_BUILD}* ${APP_WWW}

for f in $(find ${APP_WWW}); do
    if [ -f "${f}" ]; then
        # echo $f
        # brotli has better compression but needs patched webserver and up to date browser
        # brotli -fj $f
        gzip -9 -f $f
        # du -h ${f}.br
    fi
done
