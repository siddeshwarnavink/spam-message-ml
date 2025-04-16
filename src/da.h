#ifndef DA_H
#define DA_H

#define is_char_ptr(x) _Generic((x), char *: 1, const char *: 1, default: 0)
#define strlen_trust_me(x) strlen((const char *)(x))
#define strcpy_trust_me(dest, src) strcpy((char *)(dest), (const char *)(src))

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
		if (!(da_arr)->array) {											\
			fprintf(stderr, "Memory allocation failed\n");				\
			exit(EXIT_FAILURE);											\
		}																\
		(da_arr)->size = 0;												\
		(da_arr)->capacity = (da_capacity);								\
	} while(0)

// Append an item to a dynamic array.
// Automatically resizes when capacity is reached.
// Example:
//   da_append(&tokens, "hello");      // deep copies string
//   float *f = malloc(sizeof(float)); *f = 3.14;
//   da_append(&floats, f);            // stores pointer to float
#define da_append(da_arr, new_value) do {                               \
		if ((da_arr)->size == (da_arr)->capacity) {						\
			(da_arr)->capacity *= 2;									\
			(da_arr)->array = realloc((da_arr)->array, (da_arr)->capacity * sizeof(*(da_arr)->array)); \
			if (!(da_arr)->array) {										\
				fprintf(stderr, "Memory reallocation failed\n");		\
				exit(EXIT_FAILURE);										\
			}															\
		}																\
		if (is_char_ptr(new_value)) {									\
			(da_arr)->array[(da_arr)->size] = malloc(strlen_trust_me(new_value) + 1); \
			if (!(da_arr)->array[(da_arr)->size]) {						\
				fprintf(stderr, "Memory allocation for string failed\n"); \
				exit(EXIT_FAILURE);										\
			}															\
			strcpy_trust_me((da_arr)->array[(da_arr)->size], new_value); \
		} else {														\
			(da_arr)->array[(da_arr)->size] = new_value;				\
		}																\
		(da_arr)->size++;												\
	} while(0)

// Only free the dynamic array not its contents.
// Example: da_free_da_alone(&tokens);
#define da_free_da_alone(da_arr) do {           \
		free((da_arr)->array);					\
		(da_arr)->array = NULL;					\
		(da_arr)->size = 0;						\
		(da_arr)->capacity = 0;					\
	} while(0)

// Free a dynamic array and all its elements.
// Example: da_free(&tokens);
#define da_free(da_arr) do {							\
		for (size_t i = 0; i < (da_arr)->size; ++i) {   \
			free((da_arr)->array[i]);					\
		}                                               \
		da_free_da_alone(da_arr);                       \
	} while(0)

// Print contents of the dynamic array to stdout.
// You must pass a suitable format string, like "%s" for strings or "%.2f" for floats.
// Example:
//   da_print(&tokens, "%s");
//   da_print(&floats, "%.2f");
#define da_print(da_arr, element_fmt) do {				\
		printf("[");                                    \
		for (size_t i = 0; i < (da_arr)->size; ++i) {   \
			printf(element_fmt, (da_arr)->array[i]);	\
			if (i < (da_arr)->size - 1) printf(", ");	\
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
		if (!(out_array_name)) {										\
			fprintf(stderr, "Memory allocation for static array failed\n");	\
			exit(EXIT_FAILURE);											\
		}																\
		for (size_t i = 0; i < (da_arr)->size; ++i) {					\
			out_array_name[i] = (da_arr)->array[i];						\
		}																\
		out_array_name[(da_arr)->size] = NULL;							\
	} while(0)

#endif // !DA_H
