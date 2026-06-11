# dotty

`dotty` is a simple-to-use config and dotfiles manager with profile support, written in C++23

## Features

- **Simple Configuration**: Use a straightforward syntax to map source files to their destinations.
- **Profile Management**: Supports multiple configuration profiles (e.g., `main`, `wm`, `terminal`).
- **GitHub Integration**: Creates & synces a GitHub repository for your dotfiles

## Prerequisites

Before building and using `dotty`, ensure you have the following installed:

- **Dependencies**:
  - [xmake](https://xmake.io/) - Dotty's build tool
  - [github-cli](https://cli.github.com/) For repository management
  - [CLI11](https://github.com/CLIUtils/CLI11) For command line parsing
  - [readline](https://github.com/JuliaAttic/readline) For user input
  - [bat](https://github.com/sharkdp/bat) For file logging

## Installation

### Dotty is available in AUR
```bash
yay -S dotty
```
OR, if you use paru:
```bash
paru -S dotty
```


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
```dotty
# copy file
"/path/to/source/file" >> "relative/path/in/repo"
# copy directory
"/path/to/source/dir" >>* "relative/path/in/repo"
# link file
"/path/to/source/file" -> "relative/path/in/repo"
# link directory
"/path/to/source/dir" ->* "relative/path/in/repo"
```


Example:
```
"~/.bashrc" >> "shell/.bashrc"
"~/.config/nvim/init.lua" -> "nvim/.."  # '..' expands to source
"~/.config/my-wm-name" >>* "full-wm-config"
```


### 3. Applying configs

To sync your mapped files into the repo locally:
```bash
dotty update # dotty u
```

Push repo to github repository
```bash
dotty push
```

Pull your configs and apply so we can acquire them on another machine or to rollback
```bash
dotty pull
```

Dotty supports multiple profiles. to create one:
```bash
dotty profile new terminal-configs # dotty p n terminal-configs
```

You can switch between them
```bash
dotty profile switch main # dotty p s main
```

Or you can delete them
```bash
dotty profile delete terminal-configs # dotty p d terminal-configs
```

You can also directly open configuration instead of the manual way
```bash
dotty config # dotty c
```


### My Development Preferences and C++ conventions

- **Variables/Constants/Members**: `snake_case`
- **Functions/Methods**: `camelCase`
- **Classes/Typedefs**: `PascalCase`
- **Macros / Constexprs**: `UPPER_SNAKE_CASE`

### Primary project files

- `src/main.cpp`: Entry point.
- `core/include/core.hpp`: Common utility functions and std-lib wrappers.
- `include/common.hpp`: Global definitions, types, and includes, it's compiled to a PCH via xmake.


### License

This project is licensed under the GNU General Public License v3.0. For the full text of the license and terms of use, please refer to the [LICENSE](LICENSE) file.
