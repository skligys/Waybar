#!/bin/bash
set -e
rm -rf build/
git gc
git prune
git count-objects
