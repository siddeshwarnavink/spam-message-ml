#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

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
  float **weights; // NULL-terminated float* array
  float bias;
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
void load_csv_dataset(ht_string *dataset_table, ht_string *token_table, ht_string *items_table, da_string *stop_words) {
  FILE *file;
  char buf[BIG_BUFFER_SIZE];

  file = fopen("dataset/spam.csv", "r");
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
    nlp_process_string(text, stop_words, token_table, &tokens);

    if(tokens.size > 0) {
      item *new_item = malloc(sizeof(struct item));
      new_item->bias = 0.0f;
      new_item->is_spam = is_spam;

      ht_string_item_node *item_node = ht_node_create(text, new_item, ht_string_item_node);

      da_float_ptr empty_weights;
      da_init(&empty_weights, 32, float *);

      da_to_array(&tokens, item_node->value->tokens, float *); // Store static NULL-terminating array of tokens.

      for(size_t i = 0; i < tokens.size; ++i) {
        float *val = malloc(sizeof(float));
        *val = 0.0f;
        da_append(&empty_weights, val);
      }
      da_to_array(&empty_weights, item_node->value->weights, float *); // Store static NULL-terminating array of weights.

      da_free_da_alone(&empty_weights);
      da_free_da_alone(&tokens);

      ht_insert(items_table, item_node);
    } else {
      da_free(&tokens);
    }
  }

  fclose(file);
}

// ---------- Main program  ----------

void print_item(item *itm) {
  printf("----------\n");
  printf("weights = [");
  for(size_t i = 0; itm->weights[i] != NULL; ++i) {
    printf("%f,", *itm->weights[i]);
  }
  printf("NULL]\n");
  printf("bias = %f\n", itm->bias);
}

void free_item(void *value) {
  item *itm = (item *)value;

  assert(itm);
  assert(itm->weights);
  assert(itm->tokens);

  for(size_t i = 0; itm->tokens[i] != NULL; ++i) {
    free(itm->weights[i]);
  }

  free(itm->tokens);
  free(itm->weights);
  free(itm);
}

#define LEARNING_RATE 0.01
#define EPOCHS 1000

float sigmoidf(float x) {
  return 1.0f / (1.0f + expf(-x));
}

int main() {
  ht_string token_value_table, classification_table, items_table;
  ht_init(&token_value_table, 128, ht_string_float_node);
  ht_init(&classification_table, 256, ht_string_bool_node);
  ht_init(&items_table, 256, ht_string_item_node);

  da_string stop_words;
  da_init(&stop_words, 128, char *);
  get_stop_words(&stop_words);

  load_csv_dataset(&classification_table, &token_value_table, &items_table, &stop_words);

  da_free(&stop_words);

  // Normalize token values between 0-1
  ht_foreach(token_value_table, ht_string_float_node, {
      current->value = current->value / (float)token_value_table.size;
    });

  for(size_t x = 1; x <= EPOCHS; ++x) {
    int correctly_predicted = 0;

    ht_foreach(items_table, ht_string_item_node, {
        item *itm = current->value;
        // print_item(itm);

        float z = 0.0f;
        for(size_t i = 0; itm->tokens[i] != NULL; ++i) {
          assert(itm->weights[i]);
          z += (*itm->tokens[i]) * (*itm->weights[i]);
        }
        z += itm->bias;

        float y_cap = sigmoidf(z); // Classification predicted by the model.
        if((itm->is_spam && y_cap > 0.5f) || (!itm->is_spam && y_cap < 0.5f)) {
          ++correctly_predicted;
        }

        float y = itm->is_spam ? 1.0f : 0.0f; // Actual value.
        float bias_gradient = y_cap - y;

        for(size_t i = 0; itm->tokens[i] != NULL; ++i) {
          assert(itm->weights[i]);
          float weight_gradient = bias_gradient * *itm->tokens[i];
          *itm->weights[i] -= LEARNING_RATE * weight_gradient;
        }
        itm->bias -= LEARNING_RATE * bias_gradient;
      });

    float accuracy = ((float)correctly_predicted / (float)items_table.size) * 100.0f;
    printf("Accuracy: %f\n", accuracy);
  }

  ht_free(&token_value_table, ht_string_float_node);
  ht_free(&classification_table, ht_string_bool_node);
  ht_free2(&items_table, ht_string_item_node, free_item);

  return 0;
}
