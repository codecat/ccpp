# Codecat Preprocessor
A general purpose single-header preprocessor library.

## Supported directives
The following directives are currently supported:

* `#define <word>`
* `#undef <word>`
* `#if <condition>`
* `#elif <condition>`
* `#else`
* `#endif`
* `#include` (via `set_include_callback`)
* Other arbitrary directives (via `set_command_callback`)

## Example usage:
```cpp
static char* read_file(const char* path, size_t* out_size) { /* ... */ }

int main()
{
  // Read contents of file "SomeFile.txt" into "buffer"
  size_t size;
  char* buffer = read_file("SomeFile.txt", &size);

  // Create a preprocessor
  ccpp::processor p;

  // Add some definitions
  p.add_define("SOME_DEFINE");

  // Begin processing
  p.process(buffer, size);

  // Dump output
  printf("%s\n", buffer);

  return 0;
}
```

## Motivation
I couldn't find a good simple no-dependencies preprocessor library for general purpose use that was also permissively licensed, so I decided to make my own.

This was made primarily as a preprocessor for [Openplanet](https://openplanet.nl/)'s scripts.

## License
[MIT license](License.txt).
