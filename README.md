# cjit

**cjit** is a minimal version control system written in C++. It provides a familiar Git-like interface with commands for staging files, committing snapshots, branching, diffing, and merging — all stored in a simple flat-file format.

## Why

cjit is a teaching tool and a lightweight alternative for projects that don't need Git's full object model or network capabilities. It stores history in flat files (no binary packfiles), making it easy to inspect, hack on, and understand. Use it when you want version control that you can fully read and modify.

## Quick Start

```sh
g++ -Wall -o cjit cjit.cpp
./cjit init
echo "hello" > readme.txt
./cjit add readme.txt
./cjit commit "initial commit"
./cjit log
```

## Building

**Prerequisites:** A C++ compiler (g++ or clang++), POSIX environment (Linux, macOS, WSL).

```sh
g++ -Wall -o cjit cjit.cpp
```

## Usage

### Commands

| Command | Description |
|---------|-------------|
| `init` | Initialize a new repository |
| `add <file>` | Stage a file for commit |
| `rm <file>` | Unstage a file |
| `commit <message>` | Commit staged files |
| `log [--oneline]` | Show commit history |
| `diff [<id1> <id2>]` | Diff between commits, or last commit vs working tree |
| `branch` | List branches |
| `branch <name>` | Create a branch |
| `branch -d <name>` | Delete a branch |
| `checkout <branch_or_id>` | Switch branches or restore files from a commit |
| `status` | Show branch, staged files, and unstaged changes |
| `merge <branch>` | Merge a branch into the current branch |
| `rebase <branch>` | Rebase the current branch onto another |

### Example session

```sh
./cjit init
echo "print('hello')" > main.py
./cjit add main.py
./cjit commit "add hello script"
./cjit branch feature
./cjit checkout feature
echo "print('world')" >> main.py
./cjit commit "add world"
./cjit checkout main
./cjit merge feature
./cjit log --oneline
```

## Internal Storage

All repository data lives in `.cjit/`:

```
.cjit/
├── HEAD              Current branch or detached commit ID
├── commits.txt       Commit log (id|message|file1,file2,...)
├── staging.txt       Staged file list (one per line)
├── branches.txt      Branch pointers (name|commit_id)
└── objects/          File snapshots (<id>_<filename>)
```

## Merging

`cjit merge <branch>` combines the specified branch into the current one. When the same file differs between branches, conflict markers are written to the working copy for manual resolution:

```
<<<<<<< current-branch
content from current branch
=======
content from the merging branch
>>>>>>> other-branch
```

After resolving conflicts, stage and commit to complete the merge.

## Rebase

`cjit rebase <branch>` replays the current branch's file state on top of the target branch's tip, creating a new commit. This produces a linear history where the current branch appears to have branched off the target's latest state.

## License

MIT
