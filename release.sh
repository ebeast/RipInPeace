#!/bin/bash

if [[ ! "$2" = "" ]]; then
	debchange -v $1 "$2"
	git commit -a -m "$2"
fi

opwd=`pwd`
cd /tmp

echo Checking out...
git clone $opwd
cd RipInPeace

if [ "$1" = "" ]; then
	archdir="ripinpeace-$(date +%Y%m%d)"
else
	archdir="ripinpeace-$1"
fi
archfile="$archdir.tar.bz2"

echo Cleaning backup files...
find . | grep \~ | xargs rm -f

echo Cleaning others...
rm release.sh

echo Cleaning versioning...
rm -rf .git .gitignore

echo Renaming directory...
cd ..
mv RipInPeace $archdir

echo Creating archive...
tar c $archdir | bzip2 -- > $archfile

echo Packaging...
cd "$archdir"
./package.sh

echo Cleaning up...
rm -rf /tmp/$archdir
mv /tmp/$archfile ~

echo Done.
