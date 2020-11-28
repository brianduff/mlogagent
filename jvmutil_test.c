#include "jvmutil.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char *paramTypes;
  int num_params;
  get_param_base_types("(ZLjava/lang/String;B[S[Ljava/lang/Object;)V", &paramTypes, &num_params);
  printf("Types: %s num=%d\n", paramTypes, num_params);
  free(paramTypes);
}