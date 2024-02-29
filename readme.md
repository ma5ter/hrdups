# hrdups

`hrdups` is a C++ program designed to identify and manage duplicate files within a directory structure. It can either hardlink duplicates to save space or remove them entirely. The program uses file size and SHA-256 hash to determine duplicates.

## Features

- **Recursive Traversal**: The program traverses directories recursively to find all files.
- **Size and Hash Comparison**: Files are first compared by size, and then by SHA-256 hash to identify duplicates.
- **Hardlinking**: Duplicate files can be hardlinked to save space.
- **Removal**: Duplicate files can be removed, with an option to keep empty directories.
- **Dry Run**: A dry-run mode is available to simulate the actions without making any changes.
- **Verbose Output**: Verbose mode provides detailed output about the hashing process.

## Usage

```sh
hrdups [options] [paths]
```

### Options

- `-h`, `--help`: Show the help message and exit.
- `-k`, `--keep`: Keep empty folders on remove.
- `-p`, `--pretend`: Dry-run mode. Simulate the actions without making any changes.
- `-r`, `--remove`: Remove duplicates instead of hardlinking them.
- `-v`, `--verbose`: Explain the hashing process. Repeat the option for more verbose output.

### Example

```sh
hrdups -v -r /path/to/directory
```

This command will remove duplicate files in `/path/to/directory` with verbose output.

## Building

To build the program, you need a C++ compiler and the OpenSSL library installed.

```sh
g++ -o hrdups hrdups.cpp -lssl -lcrypto
```

## License

This program is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.

## Author

ma5ter

## Acknowledgments

- Thanks to the OpenSSL project for providing the cryptographic library.
- Thanks to the C++ community for the standard library and tools.
- Thanks to Mistral AI for creating Mistral Large 2, the Large Language Model that powers Le Chat used to write this documentation.
