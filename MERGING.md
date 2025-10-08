# Merging Instructions

Merging in upstream Luau changes is easy, since we try to limit the places we might introduce diff conflicts.

* Clone this repo
* Add a new git remote with upstream Luau (something like `git remote add upstream git@github.com:luau-lang/luau.git`)
* `git fetch upstream`
* `git merge "whichever upstream tag you want to merge"`
* Resolve conflicts if any
* Run tests with `make config=sanitize test -j12`
* Commit your changes
* Done.
