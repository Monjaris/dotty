# dotty

`dotty` is a lightweight, profile-based configuration (dotfile) manager written in C++23. It helps you organize your dotfiles, back them up to a central storage, and synchronize them with GitHub.

## Features

- **Profile Management**: Support for multiple configuration profiles (e.g., `main`, `work`, `minimal`).
- **GitHub Integration**: Automatically initialize a GitHub repository for your dotfiles using the GitHub CLI (`gh`).
- **Simple Configuration**: Use a straightforward syntax to map source files to their destinations.
- **C++23 Modernity**: Built with the latest C++ standards for performance and safety.

## Prerequisites

Before building and using `dotty`, ensure you have the following installed:

- **Compiler**: A C++23 compatible compiler (e.g., GCC 13+, Clang 16+).
- **Build System**: [xmake](https://xmake.io/)
- **Dependencies**:
  - [CLI11](https://github.com/CLIUtils/CLI11) (Can be installed via xmake)
- **External Tools**:
  - [git](https://git-scm.com/)
  - [gh](https://cli.github.com/) (GitHub CLI, for repository initialization)

## Installation

### Building from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/dotty.git
   cd dotty
   ```

2. Build the project using xmake:
   ```bash
   xmake f -m release
   xmake
   ```

3. (Optional) Install the binary:
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
- Initialize a local git repository in `~/.local/share/dotty/<profile>`.
- Create a corresponding repository on GitHub and push the initial commit.


### 2. Configuration

`dotty` looks for configuration in `~/.config/dotty/<profile>/config`. 

The configuration file uses a simple mapping syntax:
```
"/path/to/source/file" >> "relative/path/in/storage"
```


Example:
```
"~/.zshrc" >> "zsh/zshrc"
"~/.config/nvim/init.lua" >> "nvim/init.lua"
```


### 3. Synchronizing (Writing)

To copy your local files into the `dotty` storage:

```bash
dotty write
```


This will parse your config file and update the files in `~/.local/share/dotty/<profile>` based on the mappings.


## Development Conventions


### Header Files
- `*.hpp`: Headers that have (or are planned to have) implementation in a corresponding `*.cpp`.
- `*.h`: Headers that do not need a separate implementation file (templates, inline functions, constants, etc.).


### Symbols & Tokens
- **Variables/Constants/Members**: `snake_case`
- **Functions/Methods**: `camelCase`
- **Classes/Typedefs**: `PascalCase`
- **Macros / Constexprs**: `UPPER_SNAKE_CASE`


## Project Structure

- `src/main.cpp`: Entry point.
- `src/cli.cpp/hpp`: Command-line interface and subcommand implementations.
- `src/cfman.hpp`: Core configuration manager logic.
- `src/core.h`: Common utility functions and stream wrappers.
- `src/common.h`: Global definitions, types, and includes.


## License

GPLv2
