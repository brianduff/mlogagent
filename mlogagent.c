#include <jvmti.h>
#include <jni.h>
#include <classfile_constants.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>

#include "config.h"
#include "strutil.h"
#include "jvmutil.h"

// Sorry for the disgusting use of globals.
int registered = 0;
FILE *fout;
Config *config;

bool in_prepare = false;

// Store the formatted string of time in the output
void print_time() {
  struct timeval time;
  gettimeofday(&time, NULL);

  time_t rawtime = time.tv_sec;
  struct tm * timeinfo;
  timeinfo = localtime(&rawtime);

  int millis = time.tv_usec / 1000;

  buffer_append("%02d:%02d:%02d.%03d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, millis);
}

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

void checkError(jvmtiError error) {
  if (error != JVMTI_ERROR_NONE) {
    fprintf(stderr, "mlogagent: JVMTI error %d\n", error);
  }
  assert(error == JVMTI_ERROR_NONE);
}


void writeStringToBuffer(JNIEnv *jni, jobject javaString) {
  jboolean isCopy;
  const char *converted = (*jni)->GetStringUTFChars(jni, javaString, &isCopy);
  buffer_append("%s", converted);
  (*jni)->ReleaseStringUTFChars(jni, javaString, converted);
}

// Called when a breakpoint is hit.
void JNICALL BreakpointCallback(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thread, jmethodID method, jlocation location) {
  ClassConfig *classConfig = NULL;
  MethodConfig *methodConfig = NULL;

  findConfig(method, &classConfig, &methodConfig);
  if (classConfig == NULL || methodConfig == NULL) {
    fprintf(stderr, "mlogagent: Hit unknown breakpoint\n.");
    return;
  }

  buffer_reset();
  print_time();
  buffer_append(" %s(", methodConfig->method->name);

  // If the method config has showAllParams, do special things. Could make this the default in the future.
  if (methodConfig->showAllParams) {
    // Get the method modifiers. If it's static, then we start counting at 0, else we start counting at 1
    // (so as to avoid the synthetic this parameter)
    jint modifiers;
    (*jvmti_env)->GetMethodModifiers(jvmti_env, method, &modifiers);

    int start_param = ((modifiers & JVM_ACC_STATIC) != 0) ? 0 : 1;

    char *paramTypes;
    int num_params;
    // We should do this once when the config is loaded.
    get_param_base_types(methodConfig->method->signature, &paramTypes, &num_params);

    jclass strClazz = (*jni)->FindClass(jni, "java/lang/String");
    jmethodID strValueOfMethod = (*jni)->GetStaticMethodID(jni, strClazz, "valueOf", "(Ljava/lang/Object;)Ljava/lang/String;");


    for (int i = 0; i < num_params; i++) {
      jobject param;
      // Try just unconditionally getting it as an object.
      checkError((*jvmti_env)->GetLocalObject(jvmti_env, thread, 0, i + start_param, &param));
      jobject str = (jstring) (*jni)->CallStaticObjectMethod(jni, strClazz, strValueOfMethod, param);
      writeStringToBuffer(jni, str);

      if (i < num_params - 1) {
        buffer_append(", ");
      }
    }
  } else {

    // If the method config has a displayField, use that instead to get the "this" object.
    int parameter_pos = methodConfig->parameterPosition;
    if (methodConfig->displayField != NULL) {
      parameter_pos = 0;
    }

    // Get hold of the parameter
    jobject the_parameter;
    int err = (*jvmti_env)->GetLocalObject(jvmti_env, thread, 0, parameter_pos, &the_parameter);
    if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "mlogagent: GetLocalObject error: %d while trying to retrieve parameter %d of %s\n", err, parameter_pos, methodConfig->method->name);
      return;
    }

    jstring result;

    // If the method config has a displayField, look up and reflect that field.
    if (methodConfig->displayField != NULL) {
      jclass clazz = (*jni)->GetObjectClass(jni, the_parameter);
      jfieldID fieldID = (*jni)->GetFieldID(jni, clazz, methodConfig->displayField->name, methodConfig->displayField->signature);
      if (fieldID == NULL) {
        (*jni)->ExceptionClear(jni);
        fprintf(stderr, "mlogagent: Field %s:%s not found in class %s\n", methodConfig->displayField->name, methodConfig->displayField->signature, classConfig->name);
        return;
      }
      jobject value = (*jni)->GetObjectField(jni, the_parameter, fieldID);
      if (value == NULL) {
        fprintf(stderr, "mlogagent: Field %s:%s in class %s is NULL\n", methodConfig->displayField->name, methodConfig->displayField->signature, classConfig->name);
      }

      // Now toString() the value to get a String back.
      jclass strClazz = (*jni)->FindClass(jni, "java/lang/String");
      jmethodID strValueOfMethod = (*jni)->GetStaticMethodID(jni, strClazz, "valueOf", "(Ljava/lang/Object;)Ljava/lang/String;");
      result = (jstring) (*jni)->CallStaticObjectMethod(jni, strClazz, strValueOfMethod, value);
    } else {
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
    }

    if (result != NULL) {
      writeStringToBuffer(jni, result);
    }
  }
  buffer_append(")");

  fprintf(fout, "%s\n", buffer_get());
  
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


// Attach a breakpoint to configured methods
void attachMethodBreakpoints(jvmtiEnv *jvmti_env, JNIEnv *jni, jclass clazz, ClassConfig *classConfig) {
  MethodConfig *methodConfig = classConfig->method_list;
  while (methodConfig != NULL) {
    jmethodID mid = (*jni)->GetMethodID(jni, clazz, methodConfig->method->name, methodConfig->method->signature);
    methodConfig->runtimeData = mid;
    if (mid != NULL) {
      printf("Setting breakpoint for %s.%s%s\n", classConfig->name, methodConfig->method->name, methodConfig->method->signature);
      checkError((*jvmti_env)->SetBreakpoint(jvmti_env, mid, 0));
    } else {
      fprintf(stderr, "mlogagent: Can't find the method: %s.%s%s\n", classConfig->name, methodConfig->method->name, methodConfig->method->signature);
    }

    methodConfig = methodConfig->next;
  }
}

// Called when a class is loaded.
void JNICALL ClassPrepareCallback(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thread, jclass klass) {
  if (config->runtime_all_attached) {
    return;
  }

  // We could probably cache this?
  jclass clzclz = (*jni)->FindClass(jni, "java/lang/Class");
  jmethodID get_name = (*jni)->GetMethodID(jni, clzclz, "getName", "()Ljava/lang/String;");
  jstring class_name = (*jni)->CallObjectMethod(jni, klass, get_name);
  jboolean isCopy;
  const char *className = (*jni)->GetStringUTFChars(jni, class_name, &isCopy);


  ClassConfig *classConfig = config->class_list;
  bool allAttached = true;
  while (classConfig != NULL) {
    if (!classConfig->runtime_attached) {
      if (strcmp(className, classConfig->name) == 0) {
        attachMethodBreakpoints(jvmti_env, jni, klass, classConfig);
        classConfig->runtime_attached = true;
      } else {
        allAttached = false;
      }
    }
    classConfig = classConfig->next;
  }
  if (allAttached) {
    config->runtime_all_attached = true;
  }

  (*jni)->ReleaseStringUTFChars(jni, class_name, className);
  (*jni)->DeleteLocalRef(jni, clzclz);
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