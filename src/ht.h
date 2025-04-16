#ifndef HT_H
#define HT_H

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
		hash = ((hash << 5) + hash) + key[i];
	}
	return hash % table->capacity;
}

// Initialize the hash table
// Example:
//   ht_string table;
//   ht_init(&table, 64, ht_string_float_node, NULL);
#define ht_init(table, init_capacity, node_type) do {					\
		(table)->array = calloc((init_capacity), sizeof(node_type *));  \
		(table)->size = 0;                                              \
		(table)->capacity = (init_capacity);                            \
	} while (0)

// Create a node with the given key and value
// Example:
//   ht_string_float_node *node = ht_node_create("math", 91.5f, ht_string_float_node);
//   ht_insert(&table, node);
#define ht_node_create(node_key, node_value, node_type) ({				\
			node_type *node = (node_type *)malloc(sizeof(node_type));	\
			if (is_char_ptr(node_key)) {								\
				node->key = strdup(node_key);                           \
			} else {													\
				node->key = (node_key);                                 \
			}															\
			node->value = (node_value);									\
			node;														\
		})

// Insert a node into the hash table
// Automatically handles chaining via linked list
// Example:
//   ht_insert(&table, node);
#define ht_insert(table, insert_node) do {						\
		size_t index = hf_string((table), (insert_node)->key);  \
		(insert_node)->next = NULL;                             \
		if ((table)->array[index]) {                            \
			(insert_node)->next = (table)->array[index];		\
		}                                                       \
		(table)->array[index] = (insert_node);                  \
		(table)->size++;                                        \
	} while (0)

// Retrieve a pointer to the value of the key.
// Returns NULL pointer if key is not found
// Example:
//   float *score = (float *)ht_retrive(&table, "math");
#define ht_retrive(table, retrive_key, node_type) ({			\
			size_t index = hf_string((table), (retrive_key));	\
			node_type *current = (table)->array[index];			\
			void *result = NULL;								\
			while (current) {									\
				if (strcmp((retrive_key), current->key) == 0) { \
					result = &current->value;					\
					break;										\
				}                                               \
				current = current->next;                        \
			}													\
			result;												\
		})

// Free all memory used by the hash.
#define ht_free(table, node_type) do {								\
		for (size_t i = 0; i < (table)->capacity; ++i) {			\
			node_type *current = (node_type *)(table)->array[i];	\
			while (current) {										\
				node_type *temp = current;                          \
				current = current->next;                            \
				free(temp->key);                                    \
				free(temp);                                         \
			}														\
		}															\
		free((table)->array);										\
		(table)->array = NULL;										\
		(table)->size = 0;											\
		(table)->capacity = 0;										\
	} while (0)

// Free all memory used by the hash. Uses free_fn to free each value of the hash
// table.
#define ht_free2(table, node_type, free_fn) do {					\
		for (size_t i = 0; i < (table)->capacity; ++i) {			\
			node_type *current = (node_type *)(table)->array[i];	\
			while (current) {										\
				node_type *temp = current;                          \
				current = current->next;                            \
				free_fn((void *)temp->value);						\
				free(temp->key);                                    \
				free(temp);                                         \
			}														\
		}															\
		free((table)->array);										\
		(table)->array = NULL;										\
		(table)->size = 0;											\
		(table)->capacity = 0;										\
	} while (0)

// Loop through each item in the hash table.
// Example:
//   ht_foreach(token_value_table, ht_string_float_node, {
//     current->value = current->value / (float)token_value_table.size;
//   });
#define ht_foreach(table, node_type, body)          \
	for (size_t i = 0; i < (table).capacity; ++i) {	\
		node_type *current = (table).array[i];		\
		while (current) {							\
			body;									\
			current = current->next;				\
		}											\
	}

#endif // !HT_H
