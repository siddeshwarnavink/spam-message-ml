#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define SMALL_BUFFER_SIZE 64
#define BIG_BUFFER_SIZE 256

// ---------- Dynamic Array ----------
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

// ---------- Hash table ----------

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

typedef struct {
  void **array;
  size_t size;
  size_t capacity;
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

#define ht_node_create(node_key, node_value, node_type, value_type) ({  \
      node_type *node = (node_type *)malloc(sizeof(node_type));         \
      node->key = strdup(node_key);                                     \
      node->value = (value_type)(node_value);                           \
      node;                                                             \
    })

#define ht_init(table, init_capacity, node_type) do {               \
    (table)->array = calloc((init_capacity), sizeof(node_type *));  \
    (table)->size = 0;                                              \
    (table)->capacity = (init_capacity);                            \
  } while (0)

#define ht_insert(table, insert_node) do {                  \
    size_t index = hf_string((table), (insert_node)->key);  \
    (insert_node)->next = NULL;                             \
    if ((table)->array[index]) {                            \
      (insert_node)->next = (table)->array[index];          \
    }                                                       \
    (table)->array[index] = node;                           \
    (table)->size++;                                        \
  } while (0)

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

#define ht_free(table) do {                                 \
    for (size_t i = 0; i < (table)->capacity; ++i) {        \
      ht_string_float_node *current = (table)->array[i];    \
      while (current) {                                     \
        ht_string_float_node *temp = current;               \
        current = current->next;                            \
        free(temp->key);                                    \
        free(temp);                                         \
      }                                                     \
    }                                                       \
    free((table)->array);                                   \
  } while (0)

#define ht_string_print(table, node_type, format_specifier, value_cast) ({ \
      printf("--------------------------\n");                           \
      for (size_t i = 0; i < (table)->capacity; ++i) {                  \
        printf("%zu: ", i);                                             \
        node_type *current = (table)->array[i];                         \
        if (!current) {                                                 \
          printf("NULL");                                               \
        } else {                                                        \
          while (current) {                                             \
            printf(" -> (%s, " format_specifier ")",                    \
                   current->key, (value_cast)current->value);           \
            current = current->next;                                    \
          }                                                             \
        }                                                               \
        printf("\n");                                                   \
      }                                                                 \
      printf("--------------------------\n");                           \
    })

// ---------- NLP  ----------

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

char *diy_strlwr(char *str) {
  char *p = str;
  while (*p) {
    *p = tolower((unsigned char)*p);
    ++p;
  }
  return str;
}

void add_tokens_to_table(ht_string *token_table, const char *str) {
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
    ht_string_float_node *node = ht_node_create(buf, (float)token_table->size + 1, ht_string_float_node, float);
    ht_insert(token_table, node);
  }
}

void nlp_process_string(char *input, da_string *stop_words, ht_string *token_table) {
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
    case '\'':
      continue;
      break;
    case ' ':
      buf[j] = '\0';
      if(accept_string(stop_words, buf)) {
        add_tokens_to_table(token_table, diy_strlwr(buf));
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
    add_tokens_to_table(token_table, diy_strlwr(buf));
  }
}

// ---------- CSV parser  ----------

void load_csv_dataset(ht_string *dataset_table, ht_string *token_table, da_string *stop_words) {
  FILE *file;
  char buf[BIG_BUFFER_SIZE];

  file = fopen("dataset/spam.csv", "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  // int i = 0;
  while (fgets(buf, BIG_BUFFER_SIZE, file) != NULL) {
    // if(i >= 10) break;
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
    ht_string_bool_node *node = ht_node_create(buf, is_spam, ht_string_bool_node, bool);
    ht_insert(token_table, node);
    nlp_process_string(buf, stop_words, token_table);
  }

  fclose(file);
}

// ---------- Main program  ----------

int main() {
  ht_string token_table, dataset_table;
  ht_init(&token_table, 64, ht_string_float_node);
  ht_init(&dataset_table, 64, ht_string_bool_node);

  da_string stop_words;
  da_string_init(&stop_words, 128);
  get_stop_words(&stop_words);

  load_csv_dataset(&dataset_table, &token_table, &stop_words);

  // Normalize token values
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

  ht_free(&token_table);
  ht_free(&dataset_table);
}
