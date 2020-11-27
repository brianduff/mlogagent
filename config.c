#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_SIZE 255
#define INDENT_SIZE 4

#define CLASS_INDENT_LEVEL 0
#define METHOD_INDENT_LEVEL INDENT_SIZE
#define METHOD_PROPS_INDENT_LEVEL (INDENT_SIZE * 2)

// const char *DEFAULT_DISPLAY_METHOD = "toString";
// const char *DEFAULT_DISPLAY_SIG = "()Ljava/lang/String;";

typedef struct ParseState {
  ClassConfig *current_class;
  MethodConfig *current_method;
} ParseState;


char *trim(char *text, int *leading_spaces) {
  *leading_spaces = 0;
  while(isspace(*text))  {
    *leading_spaces = *leading_spaces + 1;
    text++;
  }

  if (*text == 0) {
    return text;
  }

  char *end = text + strlen(text) - 1;
  while (end > text && isspace(*end)) end--;

  end[1] = '\0';

  return text;
}

bool parseMethod(char *text, Method *method) {
    // Break up by parenthesis
  char *methodName = strtok(text, "(");
  if (methodName == NULL) {
    fprintf(stderr, "Can't find method name\n");
    return false;
  }

  // If the method name contains a period, then it's a static method, and we break it up to


  char *methodSignature = strtok(NULL, "(");
  if (methodSignature == NULL) {
    fprintf(stderr, "Can't find method signature\n");
    return false;
  }

  method->name = (char *) malloc(sizeof(char) * (strlen(methodName) + 1));
  strcpy(method->name, methodName);

  method->signature = (char *) malloc(sizeof(char) * (strlen(methodSignature) + 2));
  method->signature = (char *) malloc(sizeof(char) * (strlen(methodSignature) + 2));
  method->signature[0] = '(';
  strcpy(method->signature + 1, methodSignature);

  return true;
}

bool processMethodProp(ParseState *parseState, Config *config, char *line) {
  if (parseState->current_method == NULL) {
    fprintf(stderr, "Found a method prop, but not in a method!\n");
    return false;
  }

  // swallow the leading "- ". Very fragile.
  line += 2;

  // Break up the string based on a colon
  char *key = strtok(line, ":");
  if (key == NULL) {
    fprintf(stderr, "Expecting key: value\n");
    return false;
  }

  
  char *value = strtok(NULL, ":");
  if (value == NULL) {
    fprintf(stderr, "Expecting value\n");
    return false;
  }

  int ignored;
  value = trim(value, &ignored);

  if (strcmp("displayMethod", key) == 0) {
    Method *displayMethod = parseState->current_method->displayMethod;
    displayMethod->name = (char *) malloc(sizeof(char) * (strlen(value) + 1));
    displayMethod->signature = "()Ljava/lang/String;";
    strcpy(displayMethod->name, value);
  } else if (strcmp("showTrace", key) == 0) {
    if (strcmp("true", value) == 0) {
      parseState->current_method->showTrace = true;
    } else {
      parseState->current_method->showTrace = false;
    }
  } else if (strcmp("param", key) == 0) {
    int param = atoi(value);
    parseState->current_method->parameterPosition = param;
  } else if (strcmp("displayMethodStatic", key) == 0) {
    Method *displayMethod = parseState->current_method->displayMethod;
    if (!parseMethod(value, displayMethod)) {
      return false;
    }

    // Break up the method into its class and method
    char *className = strtok(displayMethod->name, ".");
    char *methodName = strtok(NULL, ".");
    if (methodName == NULL) {
      fprintf(stderr, "Expected class.name for static method");
      return false;
    }

    displayMethod->name = methodName;
    parseState->current_method->staticDisplayClass = className;
  } else {
    fprintf(stderr, "Warning: unrecognized property for method: %s\n", key);
  }

  return true;
}



bool processMethod(ParseState *parseState, Config *config, char *line) {
  if (parseState->current_class == NULL) {
    fprintf(stderr, "Found a method, but not in a class!\n");
    return false;
  }

  // Swallow "- "
  line += 2;

  Method *method = (Method *) malloc(sizeof(Method));

  bool result = parseMethod(line, method);
  if (!result) {
    free(method);
    return false;
  }

  MethodConfig *methodConfig = (MethodConfig *) malloc(sizeof(MethodConfig));
  methodConfig->method = method;
  methodConfig->next = parseState->current_class->method_list;
  parseState->current_class->method_list = methodConfig;
  parseState->current_method = methodConfig;

  methodConfig->displayMethod = (Method *) malloc(sizeof(Method));
  methodConfig->displayMethod->name = config->defaultDisplayMethodName;
  methodConfig->displayMethod->signature = config->defaultDisplayMethodSig;
  methodConfig->staticDisplayClass = NULL;
  methodConfig->showTrace = false;
  methodConfig->parameterPosition = 1;

  fprintf(stderr, "Class: %s. Method: %s\n", parseState->current_class->name, methodConfig->method->name);

  return true;
}

bool processClass(ParseState *parseState, Config *config, char *line) {
  // Swallow "- "
  line += 2;

  ClassConfig *clazz = (ClassConfig *) malloc(sizeof(ClassConfig));
  clazz->method_list = NULL;
  clazz->next = config->class_list;
  config->class_list = clazz;
  parseState->current_class = clazz;
  parseState->current_method = NULL;

  clazz->name = (char *) malloc(sizeof(char) * (strlen(line) + 1));
  clazz->runtime_attached = false;
  strcpy(clazz->name, line);

  return true;
}

bool processLine(ParseState *parseState, Config *config, char *line) {
  int leadingSpaces;
  char *trimmed = trim(line, &leadingSpaces);

  // Skip comments and blank lines
  if (strlen(trimmed) == 0) {
    return true;
  }
  if (trimmed[0] == '#') {
    return true;
  }

  // Because my parser is lame, all other lines must start with a dash.
  if (trimmed[0] != '-') {
    fprintf(stderr, "Expecting - at start of line\n");
    return false;
  }

  if (leadingSpaces == METHOD_PROPS_INDENT_LEVEL) {
    if (!processMethodProp(parseState, config, trimmed)) {
      return false;
    }
  } else if (leadingSpaces == METHOD_INDENT_LEVEL) {
    if (!processMethod(parseState, config, trimmed)) {
      return false;
    }
  } else if (leadingSpaces == CLASS_INDENT_LEVEL) {
    if (!processClass(parseState, config, trimmed)) {
      return false;
    }
  }


  return true;
}

void ReleaseMethods(Config *mainConfig, MethodConfig *config) {
  if (config->next != NULL) {
    ReleaseMethods(mainConfig, config->next);
  }

  if (config->displayMethod != NULL) {
    if (config->displayMethod->name != mainConfig->defaultDisplayMethodName) {
      free(config->displayMethod->name);
    }
    if (config->displayMethod->signature != mainConfig->defaultDisplayMethodSig) {
      free(config->displayMethod->signature);
    }
    free(config->displayMethod);
  }

  if (config->method != NULL) {
    free(config->method->name);
    free(config->method->signature);
    free(config->method);
  }

  free(config->runtimeData);

  free(config->staticDisplayClass);

  free(config);
}

void ReleaseClasses(Config *mainConfig, ClassConfig *config) {
  if (config->next != NULL) {
    ReleaseClasses(mainConfig, config->next);
  }
  ReleaseMethods(mainConfig, config->method_list);
  free(config->name);
  free(config);
}

void ReleaseConfig(Config *config) {
  ReleaseClasses(config, config->class_list);
  free(config);
}

Config *LoadConfig(const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    perror("Unable to open config file");
    return NULL;
  }

  Config *config = (Config *) malloc(sizeof(Config));
  config->runtime_all_attached = false;
  config->class_list = NULL;

  config->defaultDisplayMethodName = "toString";
  config->defaultDisplayMethodSig = "()Ljava/lang/String;";

  ParseState parseState;

  char buffer[MAX_LINE_SIZE];
  int line = 0;
  while(fgets(buffer, MAX_LINE_SIZE, fp)) {
    line++;
    if (!processLine(&parseState, config, buffer)) {
      // We hit a parse error. 
      fprintf(stderr, "Failed to parse %s at %d\n", filename, line);
      ReleaseConfig(config);
      fclose(fp);
      return NULL;
    }
  }

  fclose(fp);

  return config;
}

