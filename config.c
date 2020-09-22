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
    parseState->current_method->displayMethod = (char *) malloc(sizeof(char) * (strlen(value) + 1));
    strcpy(parseState->current_method->displayMethod, value);
  } else if (strcmp("showTrace", key) == 0) {
    if (strcmp("true", value) == 0) {
      parseState->current_method->showTrace = true;
    } else {
      parseState->current_method->showTrace = false;
    }
  } else if (strcmp("param", key) == 0) {
    int param = atoi(value);
    parseState->current_method->parameterPosition = param;
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

  MethodConfig *method = (MethodConfig *) malloc(sizeof(MethodConfig));
  method->next = parseState->current_class->method_list;
  parseState->current_class->method_list = method;
  parseState->current_method = method;

  // Break up by parenthesis
  char *methodName = strtok(line, "(");
  if (methodName == NULL) {
    fprintf(stderr, "Can't find method name\n");
    return false;
  }

  char *methodSignature = strtok(NULL, "(");
  if (methodSignature == NULL) {
    fprintf(stderr, "Can't find method signature\n");
    return false;
  }

  method->name = (char *) malloc(sizeof(char) * (strlen(methodName) + 1));
  strcpy(method->name, methodName);

  method->signature = (char *) malloc(sizeof(char) * (strlen(methodSignature) + 2));
  method->signature[0] = '(';
  strcpy(method->signature + 1, methodSignature);

  method->displayMethod = NULL;
  method->showTrace = false;
  method->parameterPosition = 1;

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

void ReleaseMethods(MethodConfig *config) {
  if (config->next != NULL) {
    ReleaseMethods(config->next);
  }
  free(config->name);
  free(config->signature);
  free(config->displayMethod);
  free(config->runtimeData);

  free(config);
}

void ReleaseClasses(ClassConfig *config) {
  if (config->next != NULL) {
    ReleaseClasses(config->next);
  }
  ReleaseMethods(config->method_list);
  free(config->name);
  free(config);
}

void ReleaseConfig(Config *config) {
  ReleaseClasses(config->class_list);
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

