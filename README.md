# cjit - A simple version control system

A minimal git-like VCS written in C.

## Building

```sh
gcc -Wall -o cjit cjit.c
```

## Usage

| Command | Description |
|---------|-------------|
| `cjit init` | Initialize a new repository |
| `cjit add <file>` | Stage a file for commit |
| `cjit commit <message>` | Commit staged files with a message |
| `cjit log` | Show commit history |
| `cjit diff <id1> <id2>` | Show line-by-line differences between commits |
| `cjit branch <name>` | Create a new branch from the current position |
| `cjit checkout <branch>` | Switch to an existing branch |
| `cjit checkout <id>` | Restore files from a specific commit |
| `cjit status` | Show current branch, staged files, and last commit |

## Structure

- `.cjit/commits.txt` — commit history (id|message|files)
- `.cjit/staging.txt` — staged file list
- `.cjit/branches.txt` — branch name and commit pointer
- `.cjit/HEAD` — current branch or commit
- `.cjit/objects/` — file snapshots per commit
