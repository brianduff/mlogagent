#include <classfile_constants.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "jvmutil.h"

// Given the signature of method (e.g. "(Ljava/lang/String;Z)V"), returns an array
// containing the base types of the method, e.g. "LZ", and the number of parameters.
// Returns [ for arrays.
// You must free the allocated paramTypes array.
void get_param_base_types(char *signature, char **paramTypes, int *num_params) {
  char *output = (char *) malloc(50 * sizeof(char));

  *paramTypes = output;
  int count = 0;
  bool ignore_next_type = false;

  while (*signature != 0 && *signature != JVM_SIGNATURE_ENDFUNC) {
    switch (*signature) {
      case JVM_SIGNATURE_ARRAY:
        *output = JVM_SIGNATURE_ARRAY;
        output++;
        count++;
        ignore_next_type = true;
        break;
      case JVM_SIGNATURE_BOOLEAN:
      case JVM_SIGNATURE_BYTE:
      case JVM_SIGNATURE_CHAR:
      case JVM_SIGNATURE_DOUBLE:
      case JVM_SIGNATURE_FLOAT:
      case JVM_SIGNATURE_INT:
      case JVM_SIGNATURE_LONG:
      case JVM_SIGNATURE_SHORT:
        if (!ignore_next_type) {
          *output = *signature;
          count++;
          output++;
        }
        break;
      case JVM_SIGNATURE_CLASS:
        if (!ignore_next_type) {
          *output = *signature;
          output++;
          count++;
        }
        // Keep reading until we pass the end of the class.
        while (*signature != JVM_SIGNATURE_ENDCLASS) {
          signature++;
        }
        break;
    }

    signature++;
  }

  *output = '\0';
  *num_params = count;
}