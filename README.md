# cjit — a minimal VCS

A simple version control system written in C++. Inspired by git.

## Build

```sh
g++ -Wall -o cjit cjit.cpp
```

## Usage

| Command | Description |
|---------|-------------|
| `cjit init` | Initialize a repository |
| `cjit add <file>` | Stage a file |
| `cjit rm <file>` | Unstage a file |
| `cjit commit <message>` | Commit staged files |
| `cjit log [--oneline]` | Show commit history |
| `cjit diff [<id1> <id2>]` | Diff commits or working tree |
| `cjit branch [<name>]` | List or create branches |
| `cjit branch -d <name>` | Delete a branch |
| `cjit checkout <branch\|id>` | Switch branch or restore commit |
| `cjit status` | Show repository state |
| `cjit merge <branch>` | Merge a branch into the current branch |
| `cjit rebase <branch>` | Rebase current branch onto another |

## Storage

```
.cjit/
  HEAD              current branch or commit
  commits.txt       commit log (id|message|files)
  staging.txt       staged file list
  branches.txt      branch pointers (name|commit_id)
  objects/          file snapshots per commit

## Merge & Rebase

`cjit merge <branch>` combines the specified branch into the current one.
Conflicting files get standard markers:

```
<<<<<<< current-branch
content from current branch
=======
content from merging branch
>>>>>>> other-branch
```

`cjit rebase <branch>` replays the current branch's changes onto the target
branch's tip.
```
