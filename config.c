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

void PrintNameAndSignature(NameAndSignature *nas) {
  if (nas == NULL) {
    fprintf(stderr, "NULL,");
  } else {
    fprintf(stderr, "{name = \"%s\", signature = \"%s\"},\n", nas->name, nas->signature);
  }
}

void PrintMethodConfig(MethodConfig *methodConfig) {
  fprintf(stderr, "      {\n");
  fprintf(stderr, "        method = ");
  PrintNameAndSignature(methodConfig->method);
  fprintf(stderr, "        parameterPosition = %d,\n", methodConfig->parameterPosition);
  fprintf(stderr, "        displayMethod = ");
  PrintNameAndSignature(methodConfig->displayMethod);
  fprintf(stderr, "        staticDisplayClass = \"%s\",\n", methodConfig->staticDisplayClass);
  fprintf(stderr, "        showTrace = %d,\n", methodConfig->showTrace);
  fprintf(stderr, "        showAllParams = %d,\n", methodConfig->showAllParams);
  fprintf(stderr, "        displayField = ");
  PrintNameAndSignature(methodConfig->displayField);
  fprintf(stderr, "\n");
  fprintf(stderr, "      },\n");
}


void PrintClassConfig(ClassConfig *clsConfig) {
  fprintf(stderr, "  {\n");
  fprintf(stderr, "    name = \"%s\",\n", clsConfig->name);
  fprintf(stderr, "    runtime_attached = %d,\n", clsConfig->runtime_attached);
  fprintf(stderr, "    method_list = [\n");
  MethodConfig *methodConfig = clsConfig->method_list;
  while (methodConfig != NULL) {
    PrintMethodConfig(methodConfig);
    methodConfig = methodConfig->next;
  }
  fprintf(stderr, "    ],\n");
  fprintf(stderr, "  },\n");
}

void PrintConfig(Config *config) {
  fprintf(stderr, "{\n");
  fprintf(stderr, "  classes: [\n");

  ClassConfig *clsConfig = config->class_list;
  while (clsConfig != NULL) {
    PrintClassConfig(clsConfig);
    clsConfig = clsConfig->next;
  }

  fprintf(stderr, "  ],\n");
  fprintf(stderr, "  runtime_all_attached = %d,\n", config->runtime_all_attached);
  fprintf(stderr, "  defaultDisplayMethodName = \"%s\",\n", config->defaultDisplayMethodName);
  fprintf(stderr, "  defaultDisplayMethodSig = \"%s\",\n", config->defaultDisplayMethodSig);
  fprintf(stderr, "}\n");
}


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

// A name and signature is either:
//    someMethod(foo)bar
//    someField:bar
bool parseNameAndSignature(char *text, NameAndSignature *nameAndSig) {
  // First try breaking up by colon.
  char *name;
  char *sig;
  bool is_method = false;

  name = strtok(text, ":");
  if (name != NULL) {
    sig = strtok(NULL, ":");
    if (sig == NULL) {
      // Proceed to try as a method.
      name = NULL;
    }
  }
  
  if (name == NULL) {
    // Break up by parenthesis
    name = strtok(text, "(");
    if (name == NULL) {
      fprintf(stderr, "Can't find method name in %s\n", text);
      return false;
    }

    sig = strtok(NULL, "(");
    if (sig == NULL) {
      fprintf(stderr, "Can't find method signature in %s\n", text);
      return false;
    }
    is_method = true;
  }

  nameAndSig->name = (char *) malloc(sizeof(char) * (strlen(name) + 1));
  strcpy(nameAndSig->name, name);

  nameAndSig->signature = (char *) malloc(sizeof(char) * (strlen(sig) + (is_method ? 2 : 1)));
  char *startOfSig = nameAndSig->signature;
  if (is_method) {
    nameAndSig->signature[0] = '(';
    startOfSig = startOfSig + 1;
  }
  strcpy(startOfSig, sig);

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

  
  char *value = strtok(NULL, "");
  if (value == NULL) {
    fprintf(stderr, "Expecting value\n");
    return false;
  }

  int ignored;
  value = trim(value, &ignored);

  if (strcmp("displayMethod", key) == 0) {
    NameAndSignature *displayMethod = parseState->current_method->displayMethod;
    displayMethod->name = (char *) malloc(sizeof(char) * (strlen(value) + 1));
    displayMethod->signature = "()Ljava/lang/String;";
    strcpy(displayMethod->name, value);
  } else if (strcmp("showTrace", key) == 0) {
    if (strcmp("true", value) == 0) {
      parseState->current_method->showTrace = true;
    } else {
      parseState->current_method->showTrace = false;
    }
  } else if (strcmp("showAllParams", key) == 0) {
    if (strcmp("true", value) == 0) {
      parseState->current_method->showAllParams = true;
    } else {
      parseState->current_method->showAllParams = false;
    }
  } else if (strcmp("param", key) == 0) {
    int param = atoi(value);
    parseState->current_method->parameterPosition = param;
  } else if (strcmp("displayMethodStatic", key) == 0) {
    NameAndSignature *displayMethod = parseState->current_method->displayMethod;
    if (!parseNameAndSignature(value, displayMethod)) {
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
  } else if (strcmp("displayField", key) == 0) {
    parseState->current_method->displayField = (NameAndSignature *) malloc(sizeof(NameAndSignature));
    if (!parseNameAndSignature(value, parseState->current_method->displayField)) {
      return false;
    }
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

  NameAndSignature *method = (NameAndSignature *) malloc(sizeof(NameAndSignature));

  bool result = parseNameAndSignature(line, method);
  if (!result) {
    free(method);
    return false;
  }

  MethodConfig *methodConfig = (MethodConfig *) malloc(sizeof(MethodConfig));
  methodConfig->method = method;
  methodConfig->next = parseState->current_class->method_list;
  parseState->current_class->method_list = methodConfig;
  parseState->current_method = methodConfig;

  methodConfig->displayMethod = (NameAndSignature *) malloc(sizeof(NameAndSignature));
  methodConfig->displayMethod->name = config->defaultDisplayMethodName;
  methodConfig->displayMethod->signature = config->defaultDisplayMethodSig;
  methodConfig->staticDisplayClass = NULL;
  methodConfig->displayField = NULL;
  methodConfig->showTrace = false;
  methodConfig->showAllParams = false;
  methodConfig->parameterPosition = 1;

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

  if (config->displayField != NULL) {
    if (config->displayField->name != NULL) {
      free(config->displayField->name);
    }
    if (config->displayField->signature != NULL) {
      free(config->displayField->signature);
    }
    free(config->displayField);
  }

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

  PrintConfig(config);

  return config;
}

