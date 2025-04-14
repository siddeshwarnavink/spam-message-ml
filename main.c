#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define SMALL_BUFFER_SIZE 64
#define BIG_BUFFER_SIZE 256

#define is_char_ptr(x) _Generic((x), char *: 1, const char *: 1, default: 0)

#define strlen_trust_me(x) strlen((const char *)(x))
#define strcpy_trust_me(dest, src) strcpy((char *)(dest), (const char *)(src))

// ---------- Dynamic Array ----------

// Dynamic array for strings.
typedef struct {
  char **array;
  size_t size;
  size_t capacity;
} da_string;

// Dynamic array for float pointers.
typedef struct {
  float **array;
  size_t size;
  size_t capacity;
} da_float_ptr;

// Initialize a dynamic array.
// da_init(&my_array, initial_capacity, value_type);
// Example:
//   da_string tokens;
//   da_init(&tokens, 4, char *);
#define da_init(da_arr, da_capacity, value_type) do {                   \
    (da_arr)->array = (value_type *)malloc((da_capacity) * sizeof(value_type)); \
    if (!(da_arr)->array) {                                             \
      fprintf(stderr, "Memory allocation failed\n");                    \
      exit(EXIT_FAILURE);                                               \
    }                                                                   \
    (da_arr)->size = 0;                                                 \
    (da_arr)->capacity = (da_capacity);                                 \
  } while(0)

// Append an item to a dynamic array.
// Automatically resizes when capacity is reached.
// Example:
//   da_append(&tokens, "hello");      // deep copies string
//   float *f = malloc(sizeof(float)); *f = 3.14;
//   da_append(&floats, f);            // stores pointer to float
#define da_append(da_arr, new_value) do {                               \
    if ((da_arr)->size == (da_arr)->capacity) {                         \
      (da_arr)->capacity *= 2;                                          \
      (da_arr)->array = realloc((da_arr)->array, (da_arr)->capacity * sizeof(*(da_arr)->array)); \
      if (!(da_arr)->array) {                                           \
        fprintf(stderr, "Memory reallocation failed\n");                \
        exit(EXIT_FAILURE);                                             \
      }                                                                 \
    }                                                                   \
    if (is_char_ptr(new_value)) {                                       \
      (da_arr)->array[(da_arr)->size] = malloc(strlen_trust_me(new_value) + 1); \
      if (!(da_arr)->array[(da_arr)->size]) {                           \
        fprintf(stderr, "Memory allocation for string failed\n");       \
        exit(EXIT_FAILURE);                                             \
      }                                                                 \
      strcpy_trust_me((da_arr)->array[(da_arr)->size], new_value);      \
    } else {                                                            \
      (da_arr)->array[(da_arr)->size] = new_value;                      \
    }                                                                   \
    (da_arr)->size++;                                                   \
  } while(0)

// Only free the dynamic array not its contents.
// Example: da_free_da_alone(&tokens);
#define da_free_da_alone(da_arr) do {           \
    free((da_arr)->array);                      \
    (da_arr)->array = NULL;                     \
    (da_arr)->size = 0;                         \
    (da_arr)->capacity = 0;                     \
  } while(0)

// Free a dynamic array and all its elements.
// Example: da_free(&tokens);
#define da_free(da_arr) do {                        \
    for (size_t i = 0; i < (da_arr)->size; ++i) {   \
      free((da_arr)->array[i]);                     \
    }                                               \
    da_free_da_alone(da_arr);                       \
  } while(0)

// Print contents of the dynamic array to stdout.
// You must pass a suitable format string, like "%s" for strings or "%.2f" for floats.
// Example:
//   da_print(&tokens, "%s");
//   da_print(&floats, "%.2f");
#define da_print(da_arr, element_fmt) do {          \
    printf("[");                                    \
    for (size_t i = 0; i < (da_arr)->size; ++i) {   \
      printf(element_fmt, (da_arr)->array[i]);      \
      if (i < (da_arr)->size - 1) printf(", ");     \
    }                                               \
    printf("]\n");                                  \
  } while(0)

// Convert a dynamic array into a NULL-terminated static array.
// Caller is responsible for freeing the returned array (but not its elements).
// Example:
//   float **arr;
//   da_to_array(&floats, arr, float *);
#define da_to_array(da_arr, out_array_name, value_type) do {            \
    out_array_name = (value_type *)malloc(((da_arr)->size + 1) * sizeof(value_type)); \
    if (!(out_array_name)) {                                            \
      fprintf(stderr, "Memory allocation for static array failed\n");   \
      exit(EXIT_FAILURE);                                               \
    }                                                                   \
    for (size_t i = 0; i < (da_arr)->size; ++i) {                       \
      out_array_name[i] = (da_arr)->array[i];                           \
    }                                                                   \
    out_array_name[(da_arr)->size] = NULL;                              \
  } while(0)

// ---------- Hash table (String Keys) ----------

typedef struct ht_string_float_node {
  char *key;
  float value;
  struct ht_string_float_node *next;
} ht_string_float_node;

typedef struct ht_string_bool_node {
  char *key;
  bool value;
  struct ht_string_bool_node *next;
} ht_string_bool_node;

typedef struct ht_string_float_array_node {
  char *key;
  float **value; // NULL-terminated float* array
  struct ht_string_float_array_node *next;
} ht_string_float_array_node;

typedef struct {
  void **array;
  size_t size;
  size_t capacity;
  void (*free_value)(void *);  // Custom free function for values
} ht_string;

// DJB2 hash function
// https://theartincode.stanis.me/008-djb2/
size_t hf_string(ht_string *table, char *key) {
  size_t hash = 5381;
  for (size_t i = 0; key[i] != '\0'; ++i) {
    hash = ((hash << 5) + hash) + key[i]; // hash * 33 + key[i]
  }
  return hash % table->capacity;
}

// Initialize the hash table
// Example:
//   ht_string table;
//   ht_init(&table, 64, ht_string_float_node, NULL);
#define ht_init(table, init_capacity, node_type) do {               \
    (table)->array = calloc((init_capacity), sizeof(node_type *));  \
    (table)->size = 0;                                              \
    (table)->capacity = (init_capacity);                            \
  } while (0)

// Create a node with the given key and value
// Example:
//   ht_string_float_node *node = ht_node_create("math", 91.5f, ht_string_float_node);
//   ht_insert(&table, node);
#define ht_node_create(node_key, node_value, node_type) ({      \
      node_type *node = (node_type *)malloc(sizeof(node_type)); \
      if (is_char_ptr(node_key)) {                              \
        node->key = strdup(node_key);                           \
      } else {                                                  \
        node->key = (node_key);                                 \
      }                                                         \
      node->value = (node_value);                               \
      node;                                                     \
    })

// Insert a node into the hash table
// Automatically handles chaining via linked list
// Example:
//   ht_insert(&table, node);
#define ht_insert(table, insert_node) do {                  \
    size_t index = hf_string((table), (insert_node)->key);  \
    (insert_node)->next = NULL;                             \
    if ((table)->array[index]) {                            \
      (insert_node)->next = (table)->array[index];          \
    }                                                       \
    (table)->array[index] = (insert_node);                  \
    (table)->size++;                                        \
  } while (0)

// Retrieve a float value by key (specific to float tables)
// Returns -1 if key is not found
// Example:
//   float score = ht_retrive(&table, "math");
#define ht_retrive(table, retrive_key) ({                       \
      size_t index = hf_string((table), (retrive_key));         \
      ht_string_float_node *current = (table)->array[index];    \
      float result = -1;                                        \
      while (current) {                                         \
        if (strcmp((retrive_key), current->key) == 0) {         \
          result = current->value;                              \
          break;                                                \
        }                                                       \
        current = current->next;                                \
      }                                                         \
      result;                                                   \
    })

// Free all memory used by the hash.
#define ht_free(table, node_type) do {                      \
    for (size_t i = 0; i < (table)->capacity; ++i) {        \
      node_type *current = (node_type *)(table)->array[i];  \
      while (current) {                                     \
        node_type *temp = current;                          \
        current = current->next;                            \
        free(temp->key);                                    \
        free(temp);                                         \
      }                                                     \
    }                                                       \
    free((table)->array);                                   \
    (table)->array = NULL;                                  \
    (table)->size = 0;                                      \
    (table)->capacity = 0;                                  \
  } while (0)

// Free all memory used by the hash. Uses free_fn to free each value of the hash
// table.
#define ht_free2(table, node_type, free_fn) do {     \
    for (size_t i = 0; i < (table)->capacity; ++i) {        \
      node_type *current = (node_type *)(table)->array[i];  \
      while (current) {                                     \
        node_type *temp = current;                          \
        current = current->next;                            \
        if ((free_fn) != NULL) {                            \
          free_fn((void *)temp->value);                     \
        }                                                   \
        free(temp->key);                                    \
        free(temp);                                         \
      }                                                     \
    }                                                       \
    free((table)->array);                                   \
    (table)->array = NULL;                                  \
    (table)->size = 0;                                      \
    (table)->capacity = 0;                                  \
  } while (0)

// Print table contents for float or bool values
// Usage:
//   ht_string_print(&table, ht_string_float_node, "%f", (double));
//   ht_string_print(&table, ht_string_bool_node, "%s", (current->value ? "true" : "false"));
#define ht_string_print(table, node_type, format_specifier, value_cast) ({ \
      printf("--------------------------\n");                           \
      for (size_t i = 0; i < (table)->capacity; ++i) {                  \
        printf("%zu: ", i);                                             \
        node_type *current = (table)->array[i];                         \
        if (!current) {                                                 \
          printf("NULL");                                               \
        } else {                                                        \
          while (current) {                                             \
            printf(" -> (\"%s\", " format_specifier ")",                \
                   current->key, (value_cast)current->value);           \
            current = current->next;                                    \
          }                                                             \
        }                                                               \
        printf("\n");                                                   \
      }                                                                 \
      printf("--------------------------\n");                           \
    })

// Print table contents for array values (NULL-terminated arrays)
// Usage:
//   ht_string_array_print(&array_table, ht_string_float_array_node, "%f", double);
#define ht_string_array_print(table, node_type, format_specifier, value_cast) ({ \
      printf("--------------------------\n");                           \
      for (size_t i = 0; i < (table)->capacity; ++i) {                  \
        printf("%zu: ", i);                                             \
        node_type *current = (node_type *)(table)->array[i];            \
        if (!current) {                                                 \
          printf("NULL");                                               \
        } else {                                                        \
          while (current) {                                             \
            printf(" -> (\"%s\", [", current->key);                     \
            float **arr = (float **)(current->value);                   \
            size_t j = 0;                                               \
            while (arr[j]) {                                            \
              printf(format_specifier, (value_cast)(*arr[j]));          \
              if (arr[j + 1]) printf(", ");                             \
              j++;                                                      \
            }                                                           \
            printf("])");                                               \
            current = current->next;                                    \
          }                                                             \
        }                                                               \
        printf("\n");                                                   \
      }                                                                 \
      printf("--------------------------\n");                           \
    })


// ---------- String functions ----------

/*
 * Convert the given string to lower case.
 */
char *str_lwr(char *str) {
  char *p = str;
  while (*p) {
    *p = tolower((unsigned char)*p);
    ++p;
  }
  return str;
}

/*
 * Remove the first n characters from the given string.
 */
char *str_remove_first_chars(char *str, int n) {
  char *p = str;
  int len = strlen(str);

  if (n < len) {
    memmove(p, p + n, len - n + 1);
  } else {
    *p = '\0';
  }
  return str;
}

// ---------- NLP  ----------

/*
 * Loads the list of stop words in English into given dynamic array.
 */
void get_stop_words(da_string *words) {
  FILE *file;
  char buf[SMALL_BUFFER_SIZE];

  file = fopen("dataset/stop-words.txt", "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  while (fgets(buf, SMALL_BUFFER_SIZE, file) != NULL) {
    buf[strlen(buf) - 1] = '\0';
    da_append(words, buf);
  }

  fclose(file);
}

/*
 * Checks weather the word is a valid token, the word should be of length 3-12
 * and should not be a stop word.
 */
bool accept_string(da_string *stop_words, char *str) {
  size_t str_len = strlen(str);
  if(str_len <= 3 || str_len > 12) return false;

  for(size_t i = 0; i < stop_words->size; ++i) {
    if(strcmp(stop_words->array[i], str) == 0) {
      return false;
    }
  }
  return true;
}

/*
 * Try to expand apostrophe words, assign unique numbers to the tokens, and add
 * them to the hash table and pointers to the value in hash table to dynamic
 * array.
 */
void add_tokens_to_table(ht_string *token_table, da_float_ptr *tokens, const char *str) {
  char buf[SMALL_BUFFER_SIZE];
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
      else if((next_2char_safe && str[i + 1] == 'r' && str[i + 2] == 'e')    // 're
              || (next_2char_safe && str[i + 1] == 'v' && str[i + 2] == 'e') // 've
              || (next_2char_safe && str[i + 1] == 'l' && str[i + 2] == 'l') // 'll
              || (next_char_safe && str[i + 1] == 'd')                       // 'd
              || (next_char_safe && str[i + 1] == 's')) {                    // 's
        break;
      }
    }
    buf[j++] = str[i];
  }

  buf[j] = '\0';
  if((float)ht_retrive(token_table, buf) < 0) {
    ht_string_float_node *node = ht_node_create(buf, (float)token_table->size + 1, ht_string_float_node);
    ht_insert(token_table, node);
    da_append(tokens, &node->value);
  }
}

/*
 * The goal of this function is to assign each word of the input string a unique
 * floating value called token.
 */
void nlp_process_string(char *input, da_string *stop_words, ht_string *token_table, da_float_ptr *tokens) {
  char buf[SMALL_BUFFER_SIZE];
  size_t j = 0;

  for(size_t i = 0; i < strlen(input) && j <= SMALL_BUFFER_SIZE; ++i) {
    switch(input[i]) {
    case '.':
    case ',':
    case '?':
    case '!':
    case ':':
    case '"':
      continue;
      break;
    case ' ':
    case '(':
    case ')':
      buf[j] = '\0';
      if(accept_string(stop_words, buf)) {
        add_tokens_to_table(token_table, tokens, str_lwr(buf));
      }
      j = 0;
      break;
    default:
      if (!isdigit(input[i]) && (i - 1 >= 0 && input[i] != input[i - 1]))
        buf[j++] = input[i];
      break;
    }
  }

  buf[j] = '\0';
  if(accept_string(stop_words, buf)) {
    add_tokens_to_table(token_table, tokens, str_lwr(buf));
  }
}

// ---------- CSV parser  ----------

/*
 * Load the spam words dataset into the hash table and generate tokens for the
 * words in data set.
 */
void load_csv_dataset(ht_string *dataset_table, ht_string *token_table, ht_string *dataset_tokens_table, da_string *stop_words) {
  FILE *file;
  char buf[BIG_BUFFER_SIZE];

  file = fopen("dataset/spam.csv", "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  // int i = 0;
  while (fgets(buf, BIG_BUFFER_SIZE, file) != NULL) {
    // if(i >= 5) break;
    // ++i;
    buf[strlen(buf) - 1] = '\0';
    bool is_spam = true;
    switch(buf[0]) {
    case 'h': // "ham"
      is_spam = false;
      break;
    case 's': // "spam"
      is_spam = true;
      break;
    default:
      continue;
      break;
    }
    char *text = str_remove_first_chars(buf, is_spam ? 5 : 4);

    ht_string_bool_node *ds_node = ht_node_create(text, is_spam, ht_string_bool_node);
    ht_insert(dataset_table, ds_node);

    da_float_ptr tokens;
    ht_string_float_array_node *dst_node = ht_node_create(text, NULL, ht_string_float_array_node);

    da_init(&tokens, 32, float *);
    nlp_process_string(text, stop_words, token_table, &tokens);
    da_to_array(&tokens, dst_node->value, float *);
    da_free_da_alone(&tokens);

    ht_insert(dataset_tokens_table, dst_node);
  }

  fclose(file);
}

// ---------- Main program  ----------

void free_float_ptr_array(void *value) {
  float **arr = (float **)value;
  if (!arr) return;
  free(arr);
}

int main() {
  // --- Hash Tables ---
  // Key-value pairs of
  // token_table: token word & token float value.
  // dataset_table: input string & classification.
  // dataset_tokens_table: input string & array of tokens.
  ht_string token_table, dataset_table, dataset_tokens_table;
  ht_init(&token_table, 64, ht_string_float_node);
  ht_init(&dataset_table, 64, ht_string_bool_node);
  ht_init(&dataset_tokens_table, 64, ht_string_float_array_node);

  da_string stop_words;
  da_init(&stop_words, 128, char *);
  get_stop_words(&stop_words);

  load_csv_dataset(&dataset_table, &token_table, &dataset_tokens_table, &stop_words);

  // Normalize token values between 0-1
  for(size_t i = 0; i < token_table.capacity; ++i) {
    ht_string_float_node *current = token_table.array[i];
    while (current) {
      current->value = current->value / (float)token_table.size;
      current = current->next;
    }
  }

  printf("Dataset:\n");
  ht_string_print(&dataset_table, ht_string_bool_node, "%d", bool);

  printf("Tokens:\n");
  ht_string_print(&token_table, ht_string_float_node, "%f", float);

  printf("Dataset Tokens:\n");
  ht_string_array_print(&dataset_tokens_table, ht_string_float_array_node, "%f", double); // float is promoted in variadic functions

  da_free(&stop_words);
  ht_free(&token_table, ht_string_float_node);
  ht_free(&dataset_table, ht_string_bool_node);
  ht_free2(&dataset_tokens_table, ht_string_float_array_node, free_float_ptr_array);

  return 0;
}
