#!/usr/bin/env bash

# Run code coverage analysis
# This assumes that the build occurred with CODECOV=1 andtests have already
# fully run.

set -ex

echo "Performing code coverage analysis"
mkdir _coverage
pushd _coverage

ls -R ../build/src/

# The sed command below converts from:
#   ../build/src/libOpenImageIO/CMakeFiles/OpenImageIO.dir/foo.gcno
# to:
#   ../src/libOpenImageIO/foo.cpp

# Remove files or directories we want to exclude from code coverage analysis,
# generally because we don't expect to execute any of their code during our
# CI. (Because it's only used interactively, or is fallback code only used
# when certain dependencies are not found.) Including these when we know we
# won't be calling them during coverage gathering will just make our coverage
# percentage look artifically low.
rm -f ../build/src/iv/CMakeFiles/iv.dir/*.cpp.{gcno,gcda}
rm -f ../build/src/iv/CMakeFiles/iv.dir/iv_autogen/*.cpp.{gcno,gcda}
rm -f ../build/src/libOpenImageIO/CMakeFiles/OpenImageIO.dir/__/include/OpenImageIO/detail/pugixml/*.cpp.{gcno,gcda}


for g in $(find ../build -name "*.gcno" -type f); do
    echo "Processing $g"
    echo "dirname $g = $(dirname $g) to " `$(echo "$g" | sed -e 's/\/build\//\//' -e 's/\.gcno/\.cpp/' -e 's/\.cpp\.cpp/\.cpp/' -e 's/\/CMakeFiles.*\.dir\//\//')`
    gcov -l -p -o $(dirname "$g") $(echo "$g" | sed -e 's/\/build\//\//' -e 's/\.gcno/\.cpp/' -e 's/\/CMakeFiles.*\.dir\//\//')
done
popd
