#! /bin/bash -x
set -o errexit
set -o nounset
set -o physical

set -x

git fetch --verbose --prune vt

git checkout master
git reset --hard vt/master


git checkout mikew_devsim_schema
git rebase master
git push -f

git checkout mikew_main
git rebase master
git push -f


git fetch --verbose --prune origin

git checkout main
git rebase mikew_main
git push -f

git checkout schema
git rebase master
git push -f

git checkout devsim
git rebase schema
git push -f

git checkout future
git rebase devsim
git push -f

# vim: set noet sw=4 ts=8 ic ai:
