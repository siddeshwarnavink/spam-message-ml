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
  for (size_t i = 0; i < arr->size; ++i) {
    free(arr->array[i]);
  }
  free(arr->array);
  arr->array = NULL;
  arr->size = 0;
  arr->capacity = 0;
}

void da_string_print(da_string *arr) {
  for (size_t i = 0; i < arr->size; ++i) {
    printf("%s\n", arr->array[i]);
  }
}

void get_stop_words(da_string *words) {
  FILE *file;
  char buf[BUFFER_SIZE];

  file = fopen("dataset/stop-words.txt", "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  while (fgets(buf, BUFFER_SIZE, file) != NULL) {
    buf[strlen(buf) - 1] = '\0';
    da_string_append(words, buf);
  }

  fclose(file);
}

bool accept_string(da_string *stop_words, char *str) {
  for(size_t i = 0; i < stop_words->size; ++i) {
    if(strcmp(stop_words->array[i], str) == 0) {
      return false;
    }
  }
  return true;
}

void add_string_to_tokens(da_string *tokens, const char *str) {
  char buf[BUFFER_SIZE];
  size_t str_len = strlen(str);
  size_t j = 0;

  for(size_t i = 0; i < str_len; ++i) {
    if(str[i] == '\'') {
      bool next_char_safe = i + 1 < str_len;
      bool next_2char_safe = i + 2 < str_len;

      // n't
      if(str[i - 1] == 'n' && next_char_safe && str[i + 1] == 't') {
        buf[--j] = '\0';
        break;
      }
      else if((next_2char_safe && str[i + 1] == 'r' && str[i + 2] == 'e') // 're
              || (next_2char_safe && str[i + 1] == 'v' && str[i + 2] == 'e') // 've
              || (next_2char_safe && str[i + 1] == 'l' && str[i + 2] == 'l') // 'll
              || (next_char_safe && str[i + 1] == 'd') // 'd
              || (next_char_safe && str[i + 1] == 's')) { // 's
        break;
      }
    }
    buf[j++] = str[i];
  }

  buf[j] = '\0';
  da_string_append(tokens, buf);
}

int main() {
  char buf[BUFFER_SIZE];
  const char input[] = "don't change the following piece of code, they're used to evaluate your regex";
  size_t j = 0;

  da_string stop_words;
  da_string_init(&stop_words, 256);
  get_stop_words(&stop_words);

  da_string tokens;
  da_string_init(&tokens, 16);

  for(size_t i = 0; i < strlen(input) && j <= BUFFER_SIZE; ++i) {
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
        if(accept_string(&stop_words, buf)) {
          add_string_to_tokens(&tokens, buf);
        }
        j = 0;
        break;
      default:
        buf[j++] = input[i];
        break;
    }
  }

  buf[j] = '\0';
  if(accept_string(&stop_words, buf)) {
    add_string_to_tokens(&tokens, buf);
  }

  da_string_print(&tokens);
  da_string_free(&tokens);
}
