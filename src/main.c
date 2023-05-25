#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define REPL_BUFFER_LENGTH 1024

static void repl();
static void *readFile(const char *path);
static void runFile(const char *path);

// CURRENT 15.2

int main(int argc, char *argv[]) {

  initVM();

  if (argc == 1) {
    repl();
  }
  else if (argc == 2) {
    runFile(argv[1]);
  }
  else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  freeVM();

  return 0;
}

static void repl() {
  char line[REPL_BUFFER_LENGTH];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}

static void *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  // Move pointer to the EOF to determine the file size, then rewind it back
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Unable to read file \"%s\".\n", path);
    exit(74);
  }

  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) {
    exit(65);
  }
  if (result == INTERPRET_RUNTIME_ERROR) {
    exit(70);
  }
}
