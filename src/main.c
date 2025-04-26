#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// ---------- Configuration ----------

#define SMALL_BUFFER_SIZE 64
#define BIG_BUFFER_SIZE 256

#define VOCABULARY_SIZE 33643 // No. of unique tokens in dataset.
#define LEARNING_RATE 0.01
#define EPOCHS 10
#define TRAIN_TEST_SPLIT 75   // Percent of data to train. Rest will be used for testing.

/*
#define MAX_TOKENS_COUNT 7240 // Actual max value is 7205
#define MAX_TOKENS_LENGTH 24  // Actual max value ia 16
*/

typedef struct ht_string_uint_node {
  char *key;
  size_t value;
  struct ht_string_uint_node *next;
} ht_string_uint_node;

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
  char *text;
  bool is_spam;
  float values[VOCABULARY_SIZE];
} item;

/*
typedef struct {
  item **array;
  size_t size;
  size_t capacity;
} da_item;

typedef struct ht_string_item_node {
  char *key;
  item *value;
  struct ht_string_item_node *next;
} ht_string_item_node;
*/

typedef struct vocabulary_data {
  size_t index;
  size_t count;
} vocabulary_data;

/*
typedef struct ht_string_vocabulary_data_node {
  char *key;
  vocabulary_data *value;
  struct ht_string_vocabulary_data_node *next;
} ht_string_vocabulary_data_node;
*/

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
 * Loads the list of stop words in English into the given dynamic array.
 */
void get_stop_words(char ***words) {
  FILE *file;
  char buf[SMALL_BUFFER_SIZE];

  file = fopen("dataset/stop-words.txt", "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  while (fgets(buf, SMALL_BUFFER_SIZE, file) != NULL) {
    buf[strcspn(buf, "\r\n")] = '\0'; // Remove newline or carriage return
    arrput(*words, strdup(buf));     // Add a copy of the string to the dynamic array
  }

  fclose(file);
}

/*
 * Checks whether the word is a valid token. The word should be of length 3-12
 * and should not be a stop word.
 */
bool accept_string(char ***stop_words, char *str) {
  size_t str_len = strlen(str);
  if (str_len < 3 || str_len > 12) return false;

  for (size_t i = 0; i < arrlenu(*stop_words); ++i) {
    if (strcmp((*stop_words)[i], str) == 0) {
      return false;
    }
  }
  return true;
}

/*
 * This macro extracts token word from raw input string and allows you to
 * perform action for each word.
 */
#define extract_token_words(input, stop_words, body) do {               \
    char buf[SMALL_BUFFER_SIZE];                                        \
    size_t extract_token_words_j = 0;                                   \
    for (size_t extract_token_words_i = 0; extract_token_words_i < strlen(input) && extract_token_words_j < SMALL_BUFFER_SIZE - 1; ++extract_token_words_i) { \
      switch(input[extract_token_words_i]) {                            \
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
        buf[extract_token_words_j] = '\0';                              \
        if(accept_string(stop_words, buf)) {                            \
          body;                                                         \
        }                                                               \
        extract_token_words_j = 0;                                      \
        break;                                                          \
      default:                                                          \
        if (!isdigit(input[extract_token_words_i]) && input[extract_token_words_i] != input[extract_token_words_i - 1]) \
          buf[extract_token_words_j++] = input[extract_token_words_i];  \
        break;                                                          \
      }                                                                 \
    }                                                                   \
    buf[extract_token_words_j] = '\0';                                  \
    if (extract_token_words_j > 0 && accept_string(stop_words, buf)) {  \
      body;                                                             \
    }                                                                   \
  } while(0)

// ---------- Main program  ----------

/*
void free_item(void *value) {
  item *itm = (item *)value;
  assert(itm);
  assert(itm->tokens);
  free(itm->tokens);
  free(itm);
}
*/

/*
typedef struct model {
  size_t vocabulary_size;
  char vocabulary[VOCABULARY_SIZE];
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
*/

/*
 * Dump the model into a file.
 */
/*
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
*/

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

/*
void free_vocabulary_data(void *value) {
  vocabulary_data *itm = (vocabulary_data *)value;
  assert(itm);
  free(itm);
}
*/

void train_model(char *dataset, char *output) {
  /*
  model m;
  ht_string token_value_table, classification_table, items_table;
  ht_init(&token_value_table, 128, ht_string_float_node);
  ht_init(&classification_table, 256, ht_string_bool_node);
  ht_init(&items_table, 256, ht_string_item_node);
  */

  // Building the Vocabulary
  char **stop_words = NULL;
  get_stop_words(&stop_words);

  FILE *file;
  char buf[BIG_BUFFER_SIZE];

  file = fopen(dataset, "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  item *items = NULL;
  size_t vocabulary_i = 0;

  struct { char *key; vocabulary_data value; } *vocabulary_table = NULL;

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

    item itm;
    for (int i = 0; i < VOCABULARY_SIZE; i++) {
      itm.values[i] = 0.0f;
    }

    char *processed_text = str_lwr(str_remove_first_chars(buf, is_spam ? 5 : 4));
    itm.text = strdup(processed_text);
    itm.is_spam = is_spam;
    arrput(items, itm);

    extract_token_words(processed_text, &stop_words, {
        ptrdiff_t index = shgeti(vocabulary_table, buf);
        if (index == -1) {
          vocabulary_data data;
          data.index = vocabulary_i;
          data.count = 1;

          vocabulary_i++;
          shput(vocabulary_table, buf, data);
        } else {
          vocabulary_table[index].value.count += 1;
        }
      });
  }
  fclose(file);

  // Calculate Term Frequency (TF)
  for (size_t i = 0; i < arrlen(items); ++i) {
    size_t total_terms = 0;
    extract_token_words(items[i].text, &stop_words, {
        ptrdiff_t index = shgeti(vocabulary_table, buf);
        assert(index != -1);
        assert(vocabulary_table[index].value.index <= VOCABULARY_SIZE);
        items[i].values[vocabulary_table[index].value.index] += 1.0f;
        total_terms++;
      });

    for (size_t j = 0; j < VOCABULARY_SIZE; ++j) {
      if (items[i].values[j] > 0.0f) {
        items[i].values[j] /= (float)total_terms;
      }
    }
  }

  // Multiply by Inverse Document Frequency (IDF)
  for(size_t i = 0; i < arrlen(items); ++i) {
    extract_token_words(items[i].text, &stop_words, {
        ptrdiff_t index = shgeti(vocabulary_table, buf);
        assert(index != -1);
        assert(vocabulary_table[index].value.index <= VOCABULARY_SIZE);

        float idf = logf((float)arrlen(items) / (1 + vocabulary_table[index].value.count));
        items[i].values[vocabulary_table[index].value.index] *= idf;
      });
  }

  shfree(vocabulary_table);

  for (size_t i = 0; i < arrlen(items); i++) {
    free(items[i].text);
  }
  arrfree(items);

  for (size_t i = 0; i < arrlenu(stop_words); ++i) {
    free(stop_words[i]);
  }
  arrfree(stop_words);
  /*
  // Initialize weights and bias
  for(size_t i = 0; i < MAX_TOKENS_LENGTH; ++i)
    m.weights[i] = 0.0f;
  m.bias = 0.0f;

  size_t correctly_predicted;
  const size_t train_size = TRAIN_TEST_SPLIT * ((float)items_table.size / 100.0f);
  const size_t test_size = items_table.size - vocabulary_i;

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
  */
}

/*
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
*/

enum action {
  UNKNOWN,
  HELP,
  TRAIN,
  RUN
};

int main(int argc, char *argv[]) {
  char *dataset = "dataset/spam.csv";
  char *model = "model.bin";
  // char *input = NULL;
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
          // input = argv[x+1];
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
    // run_model(model, input);
    break;
  default:
    printf("Usage: %s [OPTION]...\n", argv[0]);
    printf("Try '%s --help' for more information.\n", argv[0]);
    return 1;
  }

  return 0;
}
