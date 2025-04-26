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

#define SMALL_BUFFER_SIZE 256
#define BUFFER_SIZE 1024

#define VOCABULARY_SIZE 8123 // No. of unique tokens in dataset.
#define LEARNING_RATE 0.001
#define LAMBDA 0.01
#define EPOCHS 5
#define TRAIN_TEST_SPLIT 70   // Percent of data to train. Rest will be used for testing.

#define HAM_WEIGHT 0.5774
#define SPAM_WEIGHT 3.7296

typedef struct item {
  char *text;
  bool is_spam;
  float values[VOCABULARY_SIZE];
} item;

typedef struct vocabulary_data {
  size_t index;
  size_t count;
} vocabulary_data;

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
    char buf[BUFFER_SIZE];                                              \
    size_t extract_token_words_j = 0;                                   \
    for (size_t extract_token_words_i = 0; extract_token_words_i < strlen(input) && extract_token_words_j < BUFFER_SIZE - 1; ++extract_token_words_i) { \
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

typedef struct model {
  float weights[VOCABULARY_SIZE];
  float bias;
  float idf[VOCABULARY_SIZE];
  char vocabulary[VOCABULARY_SIZE][16];
} model;

void load_model(model *m, const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    perror("Failed to open file for reading");
    exit(EXIT_FAILURE);
  }

  if (fread(m->weights, sizeof(float), VOCABULARY_SIZE, file) != VOCABULARY_SIZE) {
    perror("Failed to read model weights.");
    exit(EXIT_FAILURE);
  }
  if (fread(&m->bias, sizeof(float), 1, file) != 1) {
    perror("Failed to read model bias.");
    exit(EXIT_FAILURE);
  }
  if (fread(m->idf, sizeof(float), VOCABULARY_SIZE, file) != VOCABULARY_SIZE) {
    perror("Failed to read model IDF.");
    exit(EXIT_FAILURE);
  }
  for (size_t i = 0; i < VOCABULARY_SIZE; ++i) {
    if (fread(m->vocabulary[i], sizeof(char), 16, file) != 16) {
      perror("Failed to read vocabulary.");
      fclose(file);
      exit(EXIT_FAILURE);
    }
    m->vocabulary[i][15] = '\0';
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

  if (fwrite(m->weights, sizeof(float), VOCABULARY_SIZE, file) != VOCABULARY_SIZE) {
    perror("Failed to write weights");
    fclose(file);
    exit(EXIT_FAILURE);
  }
  if (fwrite(&m->bias, sizeof(float), 1, file) != 1) {
    perror("Failed to write bias");
    fclose(file);
    exit(EXIT_FAILURE);
  }
  if (fwrite(m->idf, sizeof(float), VOCABULARY_SIZE, file) != VOCABULARY_SIZE) {
    perror("Failed to write IDF");
    fclose(file);
    exit(EXIT_FAILURE);
  }
  for (size_t i = 0; i < VOCABULARY_SIZE; ++i) {
    if (fwrite(m->vocabulary[i], sizeof(char), 16, file) != 16) {
      perror("Failed to write vocabulary");
      fclose(file);
      exit(EXIT_FAILURE);
    }
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
  // Building the Vocabulary
  char **stop_words = NULL;
  get_stop_words(&stop_words);

  FILE *file;
  char buf[BUFFER_SIZE];

  file = fopen(dataset, "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  model m;

  item *items = NULL;
  size_t vocabulary_i = 0;

  struct { char *key; vocabulary_data value; } *vocabulary_table = NULL;

  fgets(buf, BUFFER_SIZE, file);
  while (fgets(buf, BUFFER_SIZE, file) != NULL) {
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
      assert(0);
      break;
    }

    item itm;
    for (int i = 0; i < VOCABULARY_SIZE; i++) {
      itm.values[i] = 0.0f;
      m.idf[i] = 0.0f;
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

          strncpy(m.vocabulary[vocabulary_i], buf, 15);
          m.vocabulary[vocabulary_i][15] = '\0';

          vocabulary_i++;
          shput(vocabulary_table, buf, data);
        } else {
          vocabulary_table[index].value.count += 1;
        }
      });
  }
  fclose(file);

  if (vocabulary_i > VOCABULARY_SIZE) {
    fprintf(stderr, "Error: Vocabulary size exceeded.\n");
    exit(1);
  }

  // Calculate Term Frequency (TF)
  for (size_t i = 0; i < arrlen(items); ++i) {
    size_t total_terms = 0;
    extract_token_words(items[i].text, &stop_words, {
        ptrdiff_t index = shgeti(vocabulary_table, buf);
        assert(index != -1);
        if (index == -1) {
          printf("Token not found in vocabulary: %s\n", buf);
          continue;
        }
        assert(vocabulary_table[index].value.index <= VOCABULARY_SIZE);
        items[i].values[vocabulary_table[index].value.index] += 1.0f;
        total_terms++;
      });

    for (size_t j = 0; j < VOCABULARY_SIZE; ++j) {
      if (items[i].values[j] > 0.0f) {
        items[i].values[j] /= (float)total_terms;
      }
    }

    if (total_terms == 0) {
      fprintf(stderr, "Warning: No valid tokens in message.\n");
      continue;
    }
  }

  // Multiply by Inverse Document Frequency (IDF)
  for(size_t i = 0; i < arrlen(items); ++i) {
    extract_token_words(items[i].text, &stop_words, {
        ptrdiff_t index = shgeti(vocabulary_table, buf);
        assert(index != -1);
        assert(vocabulary_table[index].value.index <= VOCABULARY_SIZE);

        float idf = logf((float)arrlen(items) / (1 + vocabulary_table[index].value.count));
        if(idf > 0.0f) {
          m.idf[index] = idf;
        }
        items[i].values[vocabulary_table[index].value.index] *= idf;
      });
  }

  // Training the model
  for(size_t i = 0; i < VOCABULARY_SIZE; ++i)
    m.weights[i] = 0.0f;
  m.bias = 0.0f;

  size_t true_positives = 0, false_positives = 0, false_negatives = 0;
  const size_t train_size = TRAIN_TEST_SPLIT * ((float)arrlen(items) / 100.0f);

  for(size_t x = 1; x <= EPOCHS; ++x) {
    for(size_t i = 0; i < arrlen(items); ++i) {
      float z = 0.0f;
      for(size_t j = 0; j < VOCABULARY_SIZE; ++j)
        z += items[i].values[j] * m.weights[j];
      z += m.bias;

      float y_cap = sigmoidf(z); // Classification predicted by the model.
      float y = items[i].is_spam ? 1.0f : 0.0f; // Actual value.

      if (i < train_size) { // Train
        float gradient_weight = items[i].is_spam ? SPAM_WEIGHT : HAM_WEIGHT;
        float bias_gradient = gradient_weight * (y_cap - y);

        for(size_t j = 0; j < VOCABULARY_SIZE; ++j) {
          float weight_gradient = bias_gradient * items[i].values[j];
          m.weights[j] -= LEARNING_RATE * (weight_gradient + LAMBDA * m.weights[j]);
        }
        m.bias -= LEARNING_RATE * bias_gradient;
      } else {
        if (items[i].is_spam && y_cap > 0.5f) {
          ++true_positives;
        } else if (!items[i].is_spam && y_cap > 0.5f) {
          ++false_positives;
        } else if (items[i].is_spam && y_cap <= 0.5f) {
          ++false_negatives;
        }
      }
    }
  }

  float precision = (true_positives + false_positives > 0) ?
    (float)true_positives / (true_positives + false_positives) : 0.0f;
  float recall = (true_positives + false_negatives > 0) ?
    (float)true_positives / (true_positives + false_negatives) : 0.0f;
  float f1_score = (precision + recall > 0) ?
    2.0f * (precision * recall) / (precision + recall) : 0.0f;

  printf("Precision: %.2f%%\n", precision * 100.0f);
  printf("Recall: %.2f%%\n", recall * 100.0f);
  printf("F1-Score: %.2f%%\n", f1_score * 100.0f);

  if(output != NULL) {
    dump_model(&m, output);
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

  char **stop_words = NULL;
  get_stop_words(&stop_words);

  model m;
  load_model(&m, path);

  // Calculate Term Frequency (TF)
  struct { char *key; float value; } *vocabulary_count = NULL;
  for(size_t i = 0; i < VOCABULARY_SIZE; ++i) {
    shput(vocabulary_count, m.vocabulary[i], 0.0f);
  }

  size_t total = 0;
  extract_token_words(str_lwr(input), &stop_words, {
      ptrdiff_t index = shgeti(vocabulary_count, buf);
      if (index != -1) {
        size_t current_value = vocabulary_count[index].value;
        shput(vocabulary_count, buf, current_value + 1.0f);
        total++;
      }
    });


  for (size_t i = 0; i < shlenu(vocabulary_count); ++i) {
    vocabulary_count[i].value /= (float)total;
    vocabulary_count[i].value *= m.idf[i];
  }

  float z = 0.0f;
  for (size_t i = 0; i < shlenu(vocabulary_count); ++i) {
    if (vocabulary_count[i].value > 0) {
      size_t vocab_index = shget(vocabulary_count, m.vocabulary[i]);
      z += vocabulary_count[i].value * m.weights[vocab_index];
    }
  }
  z += m.bias;

  float y_cap = sigmoidf(z);

  if(y_cap > 0.45f) {
    printf("It is spam.\n");
  } else {
    printf("It not a spam.\n");
  }

  if(should_free) free(input);

  for (size_t i = 0; i < arrlenu(stop_words); ++i) {
    free(stop_words[i]);
  }
  arrfree(stop_words);

  shfree(vocabulary_count);
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
