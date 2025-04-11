#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define BUFFER_SIZE 256

typedef struct {
  char **array;    
  size_t size;  
  size_t capacity;
} da_string;

void da_string_init(da_string *arr, size_t capacity) {
  arr->array = (char **)malloc(capacity * sizeof(char *));
  if (!arr->array) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(EXIT_FAILURE);
  }
  arr->size = 0;
  arr->capacity = capacity;
}

void da_string_append(da_string *arr, const char *str) {
  if (arr->size == arr->capacity) {
    arr->capacity *= 2;
    arr->array = (char **)realloc(arr->array, arr->capacity * sizeof(char *));
    if (!arr->array) {
      fprintf(stderr, "Memory reallocation failed\n");
      exit(EXIT_FAILURE);
    }
  }
  arr->array[arr->size] = (char *)malloc((strlen(str) + 1) * sizeof(char));
  if (!arr->array[arr->size]) {
    fprintf(stderr, "Memory allocation for string failed\n");
    exit(EXIT_FAILURE);
  }
  strcpy(arr->array[arr->size], str);
  arr->size++;
}

void da_string_free(da_string *arr) {
  for (size_t i = 0; i < arr->size; i++) {
    free(arr->array[i]); 
  }
  free(arr->array); 
  arr->array = NULL;
  arr->size = 0;
  arr->capacity = 0;
}

void da_string_print(da_string *arr) {
  for (size_t i = 0; i < arr->size; i++) {
    printf("%s\n", arr->array[i]);
  }
}

int main() {
  char buf[BUFFER_SIZE];
  const char input[] = "why did the chicken cross the road?";
  size_t j = 0;

  da_string tokens;
  da_string_init(&tokens, 256);

  for(size_t i = 0; i < strlen(input); ++i) {
    switch(input[i]) {
      case '.':
      case ',':
      case '?':
      case '!':
      case ':':
        continue;
        break;
      case ' ':
        buf[j] = '\0';
        da_string_append(&tokens, buf);
        j = 0;
        break;
      default:
        buf[j++] = input[i];
        break;
    }
  }

  buf[j] = '\0';
  da_string_append(&tokens, buf);

  da_string_print(&tokens);
  da_string_free(&tokens);
}
