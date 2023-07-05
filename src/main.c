/* A personal build of clox that follows the book Crafting Interpreters by Robert Nystrom.
This build is exeprimental by far and can deviate from the original implementation, causing
incompatibilities, bugs and broken functionality.

Build requirement:
1. cmake, at least 3.22 (3.26 is recommended)
2. build-essential (if you use debian or its derivatives) or base-devel (if you are on arch-based system)

Adjust the cmake version to that of locally installed.
*/

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#define REPL_BUFFER_LENGTH 1024

static void ioOperationError(const char *message, const char *path);
static void repl();
static void *readFile(const char *path);
static void runFile(const char *path);

static void ioOperationError(const char *message, const char *path) {
  fprintf(stderr, message, path);
  exit(74);
}

static void repl() {
  char line[REPL_BUFFER_LENGTH];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    // A scan-compile-execute pipeline entrypoint
    interpret(line);
  }
}

static void *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    ioOperationError("Could not open file \"%s\".\n", path);
  }
  // Move pointer to the EOF to determine the file size, then rewind it back.
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(fileSize + 1);
  if (buffer == NULL) {
    ioOperationError("Not enough memory to read \"%s\".\n", path);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    ioOperationError("Unable to read file \"%s\".\n", path);
  }
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);
  if (result == INTERPRET_COMPILE_ERROR) { exit(65); }
  if (result == INTERPRET_RUNTIME_ERROR) { exit(70); }
}

int main(int argc, char *argv[]) {
  initVM();

  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  freeVM();
  return 0;
}
