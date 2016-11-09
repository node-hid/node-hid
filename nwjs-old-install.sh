#!/bin/sh
#
# this is for if you have node-webkit, and even then maybe not -tod
#

set -e

nodewebkit_version() {
    grep -o '"nodewebkit":.*"\(.*\)"' package.json |\
        sed 's/.*webkit":.*"\(.*\)".*/\1/'
}

nw_gyp() {
    dir=$1
    version=$(echo $2 | sed 's/[^0-9rcRC.\-]//g')

    cd $dir
    echo "Configuring nw-gyp for target=$version"
    nw-gyp configure --target=$version
    nw-gyp build
}

[ -d build ] && node-gyp clean
if which nw-gyp >/dev/null
then
    # we've got nw-gyp... check if we're in a node-webkit project
    myDir=$PWD
    cd ..

    parentDir=${PWD##*/}
    while [ $parentDir == 'node_modules' ]
    do
        cd ..
        currentProject=${PWD##*/}
        version=$(nodewebkit_version)
        if [ a$version != a ]
        then
            nw_gyp $myDir $version
            exit
        fi

        cd ..
        parentDir=${PWD##*/}
    done

    # go back home
    cd $myDir
fi

# no? just do boring node-gyp
node-gyp configure build install
