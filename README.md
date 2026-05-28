# dotty

`dotty` is a simple-to-use config and dotfiles manager with profile support, written in C++23

## Features

- **Simple Configuration**: Use a straightforward syntax to map source files to their destinations.
- **Profile Management**: Supports multiple configuration profiles (e.g., `main`, `wm`, `terminal`).
- **GitHub Integration**: Creates & synces a GitHub repository for your dotfiles

## Prerequisites

Before building and using `dotty`, ensure you have the following installed:

- **Dependencies**:
  - [xmake] (https://xmake.io/) - Dotty's build tool
  - [GCC] (https://gcc.gnu.org) - Be sure the compiler is up-to-date
  - [git] (https://git-scm.com/) - Git command line
  - [github-cli] (https://cli.github.com/) For repository management
  - [CLI11] (https://github.com/CLIUtils/CLI11) For command line parsing
  - [readline] (https://github.com/JuliaAttic/readline) For user input
  - [bat] (https://github.com/sharkdp/bat) For file logging

## Installation

### Building from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/Monjaris/dotty.git
   cd dotty
   ```

2. Build dotty using xmake:
   ```bash
   ./build.sh
   ```

3. Install the binary:
   ```bash
   xmake install
   ```


## Usage


### 1. Initialization

To set up `dotty` for the first time:

```bash
dotty init
```

This command will:
- Check for GitHub authentication.
- Prompt for a repository name and visibility.
- Initialize a local git repository in `~/.local/share/dotty/<profile-name>`.
- Create a corresponding repository on GitHub and push the initial commit.


### 2. Configuration

`dotty` looks for configuration in `~/.config/dotty/<profile>/config`. 

The configuration file uses a simple mapping syntax:
```
"/path/to/source/file" >> "relative/path/in/storage"
```


Example:
```
"~/.bashrc" >> "shell/.bashrc"
"~/.config/nvim/init.lua" >> "nvim/init.lua"
```


### 3. Synchronizing (Writing)

To copy your local files into the `dotty` storage:

```bash
dotty write
```


This will parse your config file and update the files in `~/.local/share/dotty/<profile-name>` based on the mappings.


## Development Conventions


### Header Files
- `*.hpp`: Headers that have (or are planned to have) implementation in a corresponding `*.cpp`.
- `*.h`: Headers that do not need a separate implementation file (templates, inline functions, constants, etc.).


### Symbols & Tokens
- **Variables/Constants/Members**: `snake_case`
- **Functions/Methods**: `camelCase`
- **Classes/Typedefs**: `PascalCase`
- **Macros / Constexprs**: `UPPER_SNAKE_CASE`


## Primary project files

- `src/main.cpp`: Entry point.
- `src/core.h`: Common utility functions and stream wrappers.
- `src/common.h`: Global definitions, types, and includes.


## License

This project is licensed under the [GNU AGPLv3](./LICENSE.md) 2026 Monjaris
