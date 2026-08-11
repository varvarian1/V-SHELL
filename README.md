# V-SHELL

V-SHELL is a lightweight command-line interpreter (shell) written in C.

### Features

- **Built-in commands**: `cd`, `exit`, `echo`, `pwd`
- **Pipelines**(`|`) - connect the output of one command to the input of another
- **Redirections**:
	- `>` - overwrite a file
	- `>>` - append to a file
	- `<` - read from a file
- **Logical operators**:
	- `&&` - execute the right side only if the left succeeds
	- `||` - execute the right side only if the left fails
- **Control structures**:
	- Conditional statements (`if`/`then`/`else`/`fi`)
	- For loop (`for var in list; do ... done`) with optional `in` and positional fallback
- **History**: command history with `history` command stored in `~/.v-shell_history`
- **Script execution**: run scripts with `./script.sh` or `v-shell script.sh` 

### Requirements

- GCC compiler
- Make
- Linux

### Build and Usage

```bash
git clone git@github.com:varvarian1/V-SHELL.git
cd V-SHELL
make all
```

### Install
```bash
sudo make install
```

### Uninstall
```bash
sudo make uninstall
```

### Planned Features

- Tab completion
- Environment variables
- Heredoc `<<` support

### License
This project is licensed. See LICENSE.md
