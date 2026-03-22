# lpsh — A Unix Shell

A feature-rich Unix shell written in C, implementing a real **lexer → parser → executor** pipeline architecture. `lpsh` supports pipes, I/O redirection, job control, environment variable expansion, globbing, aliases, persistent history, and a configurable prompt with git branch detection.

## Features

| Feature | Description |
|---------|-------------|
| **Lexer/Parser** | Proper tokenizer and recursive-descent parser — not ad-hoc string splitting |
| **Pipes** | Multi-stage pipelines: `cat file \| grep pattern \| sort \| uniq -c` |
| **I/O Redirection** | `<`, `>`, `>>`, `2>`, `2>>` on any command, including builtins |
| **Logical Operators** | `&&`, `\|\|`, `;` with proper short-circuit evaluation |
| **Background Jobs** | `command &` with full job table (`jobs`, `fg`, `bg`, Ctrl+Z) |
| **Job Control** | Process groups, `SIGCHLD` reaping, `SIGTSTP`/`SIGCONT` handling |
| **Variable Expansion** | `$VAR`, `$?`, `$$`, `$!`, `${VAR}` |
| **Tilde Expansion** | `~` → `$HOME`, `~user` → that user's home |
| **Glob Expansion** | `*`, `?`, `[abc]` via `glob(3)` |
| **Command Substitution** | `` `cmd` `` and `$(cmd)` |
| **Quoting** | Single quotes (literal), double quotes (with `$` expansion), `\` escapes |
| **15+ Builtins** | `cd`, `exit`, `pwd`, `echo`, `export`, `unset`, `env`, `alias`, `unalias`, `history`, `jobs`, `fg`, `bg`, `source`, `type` |
| **Aliases** | `alias ll='ls -la'` with expansion and `unalias` |
| **History** | Persistent `~/.lpsh_history` with readline, `!!`, `!n`, `!string` expansion |
| **Tab Completion** | Command and filename completion via GNU Readline |
| **Dynamic Prompt** | `user@host:~/dir (branch) ✓ $` with ANSI colors and git branch |
| **RC File** | `~/.lpshrc` executed on startup for aliases, exports, etc. |
| **Signal Handling** | Ctrl+C interrupts current command (not the shell), Ctrl+Z suspends, Ctrl+D exits |

## Architecture

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│  Input   │───▶│  Lexer   │───▶│  Parser  │───▶│ Executor │
│(readline)│    │(tokenize)│    │(AST/cmd  │    │(fork/exec│
│          │    │          │    │  lists)  │    │  pipes)  │
└──────────┘    └──────────┘    └──────────┘    └──────────┘
      │                                              │
      ▼                                              ▼
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│  Alias   │    │ Expansion│    │ Builtins │    │   Jobs   │
│ Expand   │    │ $VAR ~ * │    │ cd/fg/bg │    │ Control  │
└──────────┘    └──────────┘    └──────────┘    └──────────┘
```

## Building

**Prerequisites:** GCC, GNU Make, GNU Readline (`libreadline`)

```bash
# Build
make

# Build with debug flags and AddressSanitizer
make debug

# Install to /usr/local/bin
sudo make install

# Clean build artifacts
make clean
```

## Usage

```bash
# Run the shell
./bin/lpsh

# Example commands
echo "Hello, World!"
ls -la | grep ".c" | wc -l
cat input.txt | sort | uniq > output.txt
export PATH="$HOME/bin:$PATH"
alias gs='git status'
sleep 100 &
jobs
fg %1
cd ~/projects && make clean && make
echo "exit code: $?"
```

## Configuration

Create `~/.lpshrc` to run commands on startup:

```bash
# ~/.lpshrc

# Aliases
alias ll='ls -la'
alias gs='git status'
alias ga='git add'
alias gc='git commit'

# Environment
export EDITOR=vim
export PATH="$HOME/bin:$PATH"
```

## Project Structure

```
src/
├── main.c          Entry point, REPL, signal setup
├── shell.h         Shared types and global state
├── lexer.c/.h      Tokenizer (quoting, operators)
├── parser.c/.h     Recursive-descent parser
├── executor.c/.h   Fork/exec, pipes, redirection
├── expand.c/.h     $VAR, ~, glob, $(cmd) expansion
├── builtins.c/.h   15 built-in commands
├── jobs.c/.h       Job table, fg/bg, process groups
├── alias.c/.h      Alias system
├── history.c/.h    Readline-based persistent history
├── prompt.c/.h     Dynamic prompt with git integration
└── rc.c/.h         RC file loader
```

## License

MIT
