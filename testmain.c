#include <stdio.h>

#include "config.h"

// A little test program to verify that config file loading works.
int main(int argc, char **argv) {
    Config *config = LoadConfig(argv[1]);

    printf("Loaded config!\n");
    ClassConfig *clazz = config->class_list;
    while (clazz != NULL) {
        printf("Class: %s\n", clazz->name);
        MethodConfig *method = clazz->method_list;
        while (method != NULL) {
            printf("  Method: %s%s\n", method->method->name, method->method->signature);
            printf("    parameter pos: %d display method: %s show_trace: %d\n",
                   method->parameterPosition,
                   method->displayMethod == NULL ? "<NULL>"
                                                 : method->displayMethod->name,
                   method->showTrace);
            method = method->next;
        }
        clazz = clazz->next;
    }

    ReleaseConfig(config);

    return 0;
}