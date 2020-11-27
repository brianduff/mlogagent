#ifndef CONFIG_H_SEEN
#define CONFIG_H_SEEN

#include <stdbool.h>

typedef struct NameAndSignature {
  // The name of the method
  char *name;
  // The signature of the method in JVM format
  char *signature;
} NameAndSignature;

// Configuration for a method breakpoint
typedef struct MethodConfig {
  NameAndSignature *method;
  // The position of the parameter to write out.
  int parameterPosition;
  // The method to call on the parameter to display. 
  NameAndSignature *displayMethod;
  // The class on which to call a static display method.
  char *staticDisplayClass;
  // If true, show a stack trace when logging this method.
  bool showTrace;

  // The name of a field to display.
  NameAndSignature *displayField;

  // If true, show all parameters for this method.
  bool showAllParams;

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

  // struct timeval time;
  // struct timezone tz;
  // gettimeofday(&time, &tz);
  // structtime_t t = localtime(time.tv_sec);

  // fprintf(out, "%d:%d:%d.%d", t.tm_hour, time.tv_sec->tm_min, time.tv_sec->tm_sec);
