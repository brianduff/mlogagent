#ifndef CONFIG_H_SEEN
#define CONFIG_H_SEEN

#include <stdbool.h>

// Configuration for a method breakpoint
typedef struct MethodConfig {
  // The name of the method
  char *name;
  // The signature of the method in JVM format
  char *signature;
  // The position of the parameter to write out.
  int parameterPosition;
  // The method to call on the parameter to display. 
  // This method must take no arguments, and must return a string.
  // If NULL, a default will be used.
  char *displayMethod;
  // If true, show a stack trace when logging this method.
  bool showTrace;

  struct MethodConfig *next;

  // Used at runtime.
  void *runtimeData;

} MethodConfig;

// Configuration for a class
typedef struct ClassConfig {
  char *name;

  // A linked list of methods to put breakpoints on.
  MethodConfig *method_list;

  struct ClassConfig *next;

  bool runtime_attached;

} ClassConfig;

typedef struct Config {
  // A linked list of classes to put breapoints in.
  ClassConfig *class_list;

  bool runtime_all_attached;

} Config;

// Loads configuration from the given file.
// Returns NULL if an error occurred parsing the file.
// The caller is responsible for calling free() on the returned config object.
Config *LoadConfig(const char *filename);

void ReleaseConfig(Config *config);

#endif /* !CONFIG_H_SEEN */
