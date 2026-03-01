#!/bin/bash
# Script to setup dependencies

set -e

# Ensure deps directory exists
mkdir -p deps
cd deps

# tinyusdz
if [ ! -d "tinyusdz" ]; then
    echo "Cloning tinyusdz (branch: lightusd)..."
    git clone -b lightusd https://github.com/lighttransport/tinyusdz.git
else
    echo "Updating tinyusdz..."
    cd tinyusdz
    git fetch origin
    git checkout lightusd
    git pull origin lightusd
    cd ..
fi

echo ""
echo "Dependencies setup complete!"
