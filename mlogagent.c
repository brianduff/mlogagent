#include <jvmti.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "config.h"

// Sorry for the disgusting use of globals.
int registered = 0;
FILE *fout;
Config *config;

bool in_prepare = false;

void findConfig(jmethodID method, ClassConfig **classConfigReturn, MethodConfig **methodConfigReturn) {
  // Figure out which class and method we're in.
  ClassConfig *classConfig = config->class_list;
  while (classConfig != NULL) {
    MethodConfig *methodConfig = classConfig->method_list;
    while (methodConfig != NULL) {
      if (method == methodConfig->runtimeData) {
        *classConfigReturn = classConfig;
        *methodConfigReturn = methodConfig;
        return;
      }
      methodConfig = methodConfig->next;
    }
    classConfig = classConfig->next;
  }

}

// Called when a breakpoint is hit.
void JNICALL BreakpointCallback(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thread, jmethodID method, jlocation location) {
  ClassConfig *classConfig = NULL;
  MethodConfig *methodConfig = NULL;
//  fprintf(stderr, "mlogagent: breakpoint\n.");
  
  findConfig(method, &classConfig, &methodConfig);
  if (classConfig == NULL || methodConfig == NULL) {
    fprintf(stderr, "mlogagent: Hit unknown breakpoint\n.");
    return;
  }

  // Get hold of the parameter
  jobject the_parameter;
  int err = (*jvmti_env)->GetLocalObject(jvmti_env, thread, 0, methodConfig->parameterPosition, &the_parameter);
  if (err != JVMTI_ERROR_NONE) {
    fprintf(stderr, "mlogagent: GetLocalObject error: %d\n", err);
    return;
  }

  jstring result;
  if (methodConfig->staticDisplayClass != NULL) {
    jclass clazz = (*jni)->FindClass(jni, methodConfig->staticDisplayClass);
    jmethodID method = (*jni)->GetStaticMethodID(jni, clazz, methodConfig->displayMethod->name, methodConfig->displayMethod->signature);
    result = (jstring) (*jni)->CallStaticObjectMethod(jni, clazz, method, the_parameter);
    (*jni)->DeleteLocalRef(jni, clazz);
  } else {
    jclass clazz = (*jni)->GetObjectClass(jni, the_parameter);
    jmethodID method = (*jni)->GetMethodID(jni, clazz, methodConfig->displayMethod->name, methodConfig->displayMethod->signature);
    result = (jstring) (*jni)->CallObjectMethod(jni, the_parameter, method);
    (*jni)->DeleteLocalRef(jni, clazz);
  }

  if (result == NULL) {
    fprintf(fout, "%s: null\n", methodConfig->method->name);
  } else {
    jboolean isCopy;
    const char *converted = (*jni)->GetStringUTFChars(jni, result, &isCopy);
    fprintf(fout, "%s: %s\n", methodConfig->method->name, converted);
    (*jni)->ReleaseStringUTFChars(jni, result, converted);
  }
  

  // Show a stack trace if we've been asked to.
  if (methodConfig->showTrace) {
    jvmtiFrameInfo frames[20];
    jint count;
    jvmtiError err;

    err = (*jvmti_env)->GetStackTrace(jvmti_env, thread, 0, 20, frames, &count);
    if (err == JVMTI_ERROR_NONE && count >= 1) {
      char *methodName;
      jclass declaringClass;

      // Consider doing this higher up if it's slow.
      jclass clzclz = (*jni)->FindClass(jni, "java/lang/Class");
      jmethodID mid_getName = (*jni)->GetMethodID(jni, clzclz, "getName", "()Ljava/lang/String;");

      for (int i = 0; i < count; i++) {
        assert(JVMTI_ERROR_NONE == (*jvmti_env)->GetMethodName(jvmti_env, frames[i].method, &methodName, NULL, NULL));
        assert(JVMTI_ERROR_NONE == (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, frames[i].method, &declaringClass));

        jstring name = (*jni)->CallObjectMethod(jni, declaringClass, mid_getName);
        jboolean isCopy;
        const char *className = (*jni)->GetStringUTFChars(jni, name, &isCopy);
        fprintf(fout, " at %s.%s\n", className, methodName);

        (*jni)->ReleaseStringUTFChars(jni, name, className);
      }
      fprintf(fout, "\n");
    }
  }

  fflush(fout);
}

void checkError(jvmtiError error) {
  if (error != JVMTI_ERROR_NONE) {
    fprintf(stderr, "mlogagent: JVMTI error %d\n", error);
  }
  assert(error == JVMTI_ERROR_NONE);
}

// Attach a breakpoint to configured methods
void attachMethodBreakpoints(jvmtiEnv *jvmti_env, JNIEnv *jni, jclass clazz, ClassConfig *classConfig) {
  MethodConfig *methodConfig = classConfig->method_list;
  while (methodConfig != NULL) {
    jmethodID mid = (*jni)->GetMethodID(jni, clazz, methodConfig->method->name, methodConfig->method->signature);
    methodConfig->runtimeData = mid;
    if (mid != NULL) {
      checkError((*jvmti_env)->SetBreakpoint(jvmti_env, mid, 0));
      printf("Set breakpoint for %s.%s%s\n", classConfig->name, methodConfig->method->name, methodConfig->method->signature);
    } else {
      fprintf(stderr, "mlogagent: Can't find the method: %s.%s%s\n", classConfig->name, methodConfig->method->name, methodConfig->method->signature);
    }

    methodConfig = methodConfig->next;
  }
}

// Called when a class is loaded.
void JNICALL ClassPrepareCallback(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thread, jclass klass) {
  // Avoid re-entrancy
  if (in_prepare) {
    return;
  }

  if (config->runtime_all_attached) {
    return;
  }

  in_prepare = true;

  // Try to find all classes we want to attach to
  ClassConfig *classConfig = config->class_list;
  bool allAttached = true;
  while (classConfig != NULL) {
    if (!classConfig->runtime_attached) {
      // Try to find the class to attach to
      jclass clazz = (*jni)->FindClass(jni, classConfig->name);
      (*jni)->ExceptionClear(jni);

      if (clazz == NULL) {
        allAttached = false;
      } else {
        attachMethodBreakpoints(jvmti_env, jni, clazz, classConfig);
        classConfig->runtime_attached = true;
        (*jni)->DeleteLocalRef(jni, clazz);
      }
    }
    classConfig = classConfig->next;
  }
  if (allAttached) {
    config->runtime_all_attached = true;
  }

  in_prepare = false;
}


JNIEXPORT jint JNICALL 
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  fout = stdout;

  // Let's see if there's a log file we want to write to.
  fprintf(stderr, "mlogagent: Got options %s\n", options);
  char *option = strtok(options, ",");
  while (option != NULL) {
    fprintf(stderr, "mlogagent: options %s\n", option);
    // Find the =
    size_t pos = strcspn(option, "=");
    if (pos != strlen(option)) {
      if (strncmp(option, "file", 4) == 0) {
        char *file = option + pos + 1;
        fprintf(stderr, "mlogagent: Writing output to file: %s\n", file);
        fout = fopen(file, "w");
      } else if (strncmp(option, "config", 6) == 0) {
        char *file = option + pos + 1;
        config = LoadConfig(file);
        fprintf(stderr, "mlogagent: Loading config from file: %s\n", file);
      } else {
        fprintf(stderr, "mlogagent: Unrecognized option: %s\n", option);
      }
    }

    option = strtok(NULL, ",");
  }
  if (config == NULL) {
    fprintf(stderr, "mlogagent: Must specify a config file with config parameter.\n");
    fclose(fout);
    return JNI_ERR;
  }

  fprintf(stderr, "mlogagent: Loaded agent\n");

  // Get an env object.
  jvmtiEnv *env;
  assert(JVMTI_ERROR_NONE == (*vm)->GetEnv(vm, (void **)&env, JVMTI_VERSION));

  // Request capabilities
  jvmtiCapabilities capabilities = { 0 };
  capabilities.can_generate_breakpoint_events = 1;
  capabilities.can_access_local_variables = 1;
  assert(JVMTI_ERROR_NONE == (*env)->AddCapabilities(env, &capabilities));

  // Register callbacks
  jvmtiEventCallbacks callbacks = { 0 };
  callbacks.ClassPrepare = &ClassPrepareCallback;
  callbacks.Breakpoint = &BreakpointCallback;
  assert(JVMTI_ERROR_NONE == (*env)->SetEventCallbacks(env, &callbacks, sizeof(callbacks)));

  // Enable events
  assert(JVMTI_ERROR_NONE == (*env)->SetEventNotificationMode(env, JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT, NULL));
  assert(JVMTI_ERROR_NONE == (*env)->SetEventNotificationMode(env, JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, NULL));

  return JNI_OK;
}

JNIEXPORT void JNICALL 
Agent_OnUnload(JavaVM *vm) {
  if (fout != stdout) {
    fclose(fout);
  }
}