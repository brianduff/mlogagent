#ifndef CONFIG_H_SEEN
#define CONFIG_H_SEEN

#include <stdbool.h>

typedef struct Method {
  // The name of the method
  char *name;
  // The signature of the method in JVM format
  char *signature;
} Method;

// Configuration for a method breakpoint
typedef struct MethodConfig {
  Method *method;
  // The position of the parameter to write out.
  int parameterPosition;
  // The method to call on the parameter to display. 
  Method *displayMethod;
  // The class on which to call a static display method.
  char *staticDisplayClass;
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

  char *defaultDisplayMethodName;
  char *defaultDisplayMethodSig;

} Config;

// Loads configuration from the given file.
// Returns NULL if an error occurred parsing the file.
// The caller is responsible for calling free() on the returned config object.
Config *LoadConfig(const char *filename);

void ReleaseConfig(Config *config);

#endif /* !CONFIG_H_SEEN */
