#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>

#include "./da.h"
#include "./ht.h"

#define SMALL_BUFFER_SIZE 64
#define BIG_BUFFER_SIZE 256

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

typedef struct item {
  float **tokens; // NULL-terminated float* array
  bool is_spam;
} item;

typedef struct ht_string_item_node {
  char *key;
  item *value;
  struct ht_string_item_node *next;
} ht_string_item_node;

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

// ---------- NLP ----------

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
  if((float *)ht_retrive(token_table, buf, ht_string_float_node) == NULL) {
    ht_string_float_node *node = ht_node_create(buf, (float)token_table->size + 1, ht_string_float_node);
    ht_insert(token_table, node);
    da_append(tokens, &node->value);
  }
}

/*
 * This macro extracts token word from raw input string and allows you to
 * perform action for each word.
 */
#define extract_token_words(input, stop_words, body) do {               \
    char buf[SMALL_BUFFER_SIZE];                                        \
    size_t j = 0;                                                       \
    for(size_t i = 0; i < strlen(input) && j <= SMALL_BUFFER_SIZE; ++i) { \
      switch(input[i]) {                                                \
      case '.':                                                         \
      case ',':                                                         \
      case '?':                                                         \
      case '!':                                                         \
      case ':':                                                         \
      case '"':                                                         \
        continue;                                                       \
        break;                                                          \
      case ' ':                                                         \
      case '(':                                                         \
      case ')':                                                         \
        buf[j] = '\0';                                                  \
        if(accept_string(stop_words, buf)) {                            \
          body;                                                         \
        }                                                               \
        j = 0;                                                          \
        break;                                                          \
      default:                                                          \
        if (!isdigit(input[i]) && (i - 1 >= 0 && input[i] != input[i - 1])) \
          buf[j++] = input[i];                                          \
        break;                                                          \
      }                                                                 \
    }                                                                   \
    buf[j] = '\0';                                                      \
    if(accept_string(stop_words, buf)) {                                \
      body;                                                             \
    }                                                                   \
  } while(0)


// ---------- CSV parser  ----------

/*
 * Load the spam words dataset into the hash table and generate tokens for the
 * words in data set.
 */
void load_csv_dataset(char *path, ht_string *dataset_table, ht_string *token_table, ht_string *items_table, da_string *stop_words) {
  FILE *file;
  char buf[BIG_BUFFER_SIZE];

  file = fopen(path, "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  while (fgets(buf, BIG_BUFFER_SIZE, file) != NULL) {
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
    da_init(&tokens, 32, float *);

    extract_token_words(text, stop_words, {
        add_tokens_to_table(token_table, &tokens, str_lwr(buf));
      });


    if(tokens.size > 0) {
      item *new_item = malloc(sizeof(struct item));
      new_item->is_spam = is_spam;

      ht_string_item_node *item_node = ht_node_create(text, new_item, ht_string_item_node);

      da_to_array(&tokens, item_node->value->tokens, float *); // Store static NULL-terminating array of tokens.
      da_free_da_alone(&tokens);

      ht_insert(items_table, item_node);
    } else {
      da_free(&tokens);
    }
  }

  fclose(file);
}

// ---------- Main program  ----------

void free_item(void *value) {
  item *itm = (item *)value;
  assert(itm);
  assert(itm->tokens);
  free(itm->tokens);
  free(itm);
}

#define LEARNING_RATE 0.01
#define EPOCHS 10000
#define MAX_TOKENS_COUNT 7240 // Actual max value is 7205
#define MAX_TOKENS_LENGTH 24  // Actual max value ia 16
#define TRAIN_TEST_SPLIT 75   // Percent of data to train. Rest will be used for testing.

typedef struct model {
  float weights[MAX_TOKENS_LENGTH];
  float bias;
  char tok_str[MAX_TOKENS_COUNT][SMALL_BUFFER_SIZE];
  float tok_val[MAX_TOKENS_COUNT];
} model;

void load_model(model *m, const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    perror("Failed to open file for reading");
    exit(EXIT_FAILURE);
  }

  if (fread(m->tok_str, sizeof(char), MAX_TOKENS_COUNT * SMALL_BUFFER_SIZE, file) != MAX_TOKENS_COUNT * SMALL_BUFFER_SIZE) {
    perror("Failed to read token strings");
    exit(EXIT_FAILURE);
  }
  if (fread(m->tok_val, sizeof(float), MAX_TOKENS_COUNT, file) != MAX_TOKENS_COUNT) {
    perror("Failed to read token values");
    exit(EXIT_FAILURE);
  }
  if (fread(m->weights, sizeof(float), MAX_TOKENS_LENGTH, file) != MAX_TOKENS_LENGTH) {
    perror("Failed to read weights");
    exit(EXIT_FAILURE);
  }
  if (fread(&m->bias, sizeof(float), 1, file) != 1) {
    perror("Failed to read bias");
    exit(EXIT_FAILURE);
  }

  fclose(file);
}

/*
 * Dump the model into a file.
 */
void dump_model(model *m, const char *path) {
  FILE *file = fopen(path, "wb");
  if (!file) {
    perror("Failed to open file for writing");
    exit(EXIT_FAILURE);
  }

  if (fwrite(m->tok_str, sizeof(char), MAX_TOKENS_COUNT * SMALL_BUFFER_SIZE, file) != MAX_TOKENS_COUNT * SMALL_BUFFER_SIZE) {
    perror("Failed to write token strings");
    exit(EXIT_FAILURE);
  }
  if (fwrite(m->tok_val, sizeof(float), MAX_TOKENS_COUNT, file) != MAX_TOKENS_COUNT) {
    perror("Failed to write token values");
    exit(EXIT_FAILURE);
  }
  if (fwrite(m->weights, sizeof(float), MAX_TOKENS_LENGTH, file) != MAX_TOKENS_LENGTH) {
    perror("Failed to write weights");
    exit(EXIT_FAILURE);
  }
  if (fwrite(&m->bias, sizeof(float), 1, file) != 1) {
    perror("Failed to write bias");
    exit(EXIT_FAILURE);
  }

  printf("Model saved to %s\n", path);
  fclose(file);
}

float sigmoidf(float x) {
  return 1.0f / (1.0f + expf(-x));
}

void print_help(char *prog) {
  printf("Usage: %s [OPTION]...\n", prog);
  printf("Train spam message detechtion machine learning model\n");
  printf("Example: %s -t -o model.bin\n", prog);

  printf("\nModel training:\n");
  printf("  -t, --train     Train the machine learning model.\n");
  printf("  -o, --output    Output file path where the model has to be stored.\n");
  printf("  -d, --dataset   Path to dataset file.\n");

  printf("\nRun model:\n");
  printf("  -r, --run       Run pre-trained model.\n");
  printf("  -m, --model     Path to model file.\n");
  printf("  -i, --input     Input string for the model.\n");
}

void train_model(char *dataset, char *output) {
  model m;
  ht_string token_value_table, classification_table, items_table;
  ht_init(&token_value_table, 128, ht_string_float_node);
  ht_init(&classification_table, 256, ht_string_bool_node);
  ht_init(&items_table, 256, ht_string_item_node);

  da_string stop_words;
  da_init(&stop_words, 128, char *);
  get_stop_words(&stop_words);

  load_csv_dataset(dataset, &classification_table, &token_value_table, &items_table, &stop_words);

  da_free(&stop_words);

  // Normalize token values between 0-1
  ht_foreach(token_value_table, ht_string_float_node, {
      current->value = current->value / (float)token_value_table.size;
    });

  // Initialize weights and bias
  for(size_t i = 0; i < MAX_TOKENS_LENGTH; ++i)
    m.weights[i] = 0.0f;
  m.bias = 0.0f;

  size_t correctly_predicted;
  const size_t train_size = TRAIN_TEST_SPLIT * ((float)items_table.size / 100.0f);
  const size_t test_size = items_table.size - train_size;

  for(size_t x = 1; x <= EPOCHS; ++x) {
    correctly_predicted = 0;
    ht_foreach(items_table, ht_string_item_node, {
        item *itm = current->value;
        float z = 0.0f;
        for(size_t i = 0; itm->tokens[i] != NULL; ++i)
          z += (*itm->tokens[i]) * m.weights[i];
        z += m.bias;

        float y_cap = sigmoidf(z); // Classification predicted by the model.
        float y = itm->is_spam ? 1.0f : 0.0f; // Actual value.

        if(x <= train_size) {
          float bias_gradient = y_cap - y;
          for(size_t i = 0; itm->tokens[i] != NULL; ++i) {
            float weight_gradient = bias_gradient * *itm->tokens[i];
            m.weights[i] -= LEARNING_RATE * weight_gradient;
          }
          m.bias -= LEARNING_RATE * bias_gradient;
        } else if((itm->is_spam && y_cap > 0.5f) || (!itm->is_spam && y_cap < 0.5f)) {
          ++correctly_predicted;
        }
      });
  }

  const float accuracy = (float)correctly_predicted / (float)items_table.size * 100.0f;
  printf("Train size: %zu\n", train_size);
  printf("Test size: %zu\n", test_size);
  printf("Correctly Predicted: %zu\n", correctly_predicted);
  printf("Accuracy: %f\n", accuracy);

  // Dump model to file
  if(output != NULL) {
    // Copy token values.
    size_t j = 0;
    ht_foreach(token_value_table, ht_string_float_node, {
        strncpy(m.tok_str[j], current->key, SMALL_BUFFER_SIZE);
        m.tok_val[j] = current->value;
        ++j;
      });

    dump_model(&m, output);
  }

  ht_free(&token_value_table, ht_string_float_node);
  ht_free(&classification_table, ht_string_bool_node);
  ht_free2(&items_table, ht_string_item_node, free_item);
}

void run_model(char *path, char *input) {
  bool should_free = false;
  if(input == NULL) {
    char buf[120];
    printf("Enter model input: ");
    fflush(stdout);
    if(read(0, buf, 120) == -1) {
      printf("Failed to read user input from stdin.");
      exit(1);
    }

    input = (char *)malloc(strlen(buf) + 1);
    if (input == NULL) {
      printf("Memory allocation failed.\n");
      exit(EXIT_FAILURE);
    }
    strcpy(input, buf);
    should_free = true;
  }

  da_string stop_words;
  da_init(&stop_words, 128, char *);
  get_stop_words(&stop_words);

  model m;
  load_model(&m, path);

  da_float_ptr tokens;
  da_init(&tokens, 32, float *);

  ht_string token_value_table;
  ht_init(&token_value_table, 128, ht_string_float_node);

  for(size_t i = 0; i < MAX_TOKENS_COUNT; ++i) {
    ht_string_float_node *node = ht_node_create(m.tok_str[i], m.tok_val[i], ht_string_float_node);
    ht_insert(&token_value_table, node);
  }

  extract_token_words(input, &stop_words, {
      float *token_value = (float *)ht_retrive(&token_value_table, str_lwr(buf), ht_string_float_node);
      if(token_value != NULL) {
        da_append(&tokens, token_value);
      }
    });

  float z = 0.0f;
  for(size_t i = 0; i < tokens.size; ++i) {
    z += *tokens.array[i] * m.weights[i];
  }
  z += m.bias;
  float y_cap = sigmoidf(z);

  if(y_cap > 0.5) {
    printf("It is spam.\n");
  } else {
    printf("It not a spam.\n");
  }

  if(should_free) free(input);
  da_free(&stop_words);
  da_free_da_alone(&tokens);
  ht_free(&token_value_table, ht_string_float_node);
}

enum action {
  UNKNOWN,
  HELP,
  TRAIN,
  RUN
};

int main(int argc, char *argv[]) {
  char *dataset = "dataset/spam.csv";
  char *model = "model.bin";
  char *input = NULL;
  enum action a = UNKNOWN;

  if (argc > 1) {
    for(size_t x = 1; x < argc; ++x) {
      if(strcmp(argv[x], "-h") == 0 || strcmp(argv[x], "--help") == 0) {
        a = HELP;
        break;
      } else if (strcmp(argv[x], "-t") == 0 || strcmp(argv[x], "--train") == 0) {
        a = TRAIN;
      } else if (strcmp(argv[x], "-o") == 0 || strcmp(argv[x], "--output") == 0) {
        if(x+1 < argc && argv[x+1][0] != '-') {
          model = argv[x+1];
        }
      } else if (strcmp(argv[x], "-d") == 0 || strcmp(argv[x], "--dataset") == 0) {
        if(x+1 < argc && argv[x+1][0] != '-') {
          dataset = argv[x+1];
        }
      } else if (strcmp(argv[x], "-r") == 0 || strcmp(argv[x], "--run") == 0) {
        a = RUN;
      } else if (strcmp(argv[x], "-m") == 0 || strcmp(argv[x], "--model") == 0) {
        if(x+1 < argc && argv[x+1][0] != '-') {
          model = argv[x+1];
        }
      } else if (strcmp(argv[x], "-i") == 0 || strcmp(argv[x], "--input") == 0) {
        if(x+1 < argc && argv[x+1][0] != '-') {
          input = argv[x+1];
        }
      }
    }
  }

  switch(a) {
  case HELP:
    print_help(argv[0]);
    break;
  case TRAIN:
    train_model(dataset, model);
    break;
  case RUN:
    run_model(model, input);
    break;
  default:
    printf("Usage: %s [OPTION]...\n", argv[0]);
    printf("Try '%s --help' for more information.\n", argv[0]);
    return 1;
  }

  return 0;
}
