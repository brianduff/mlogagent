#include <jvmti.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CLASS_TO_WATCH "frodo/Test"

const char* const break_methods[] = {
  "someMethod",
  "anotherMethod",
  0
};

const char* const break_method_signatures[] = {
  "(Lfrodo/Test$VirtualFile;)Ljava/lang/String;",
  "(Lfrodo/Test$VirtualFile;)Ljava/lang/String;",
  0
};

jmethodID break_methodIDs[] = {
  0,
  0,
  0
};

int registered = 0;

const char *GetClassName(JNIEnv *env, jclass klass) {
  jclass clzclz = (*env)->FindClass(env, "java/lang/Class");
  jmethodID mid = (*env)->GetMethodID(env, clzclz, "getName", "()Ljava/lang/String;");

   (*env)->DeleteLocalRef(env, clzclz);

  if (mid == NULL) {
    printf("Unable to load getName() method");
    return NULL;
  } else {
    jstring class_name = (jstring) (*env)->CallObjectMethod(env, klass, mid);

    jboolean isCopy;
    const char *converted = (*env)->GetStringUTFChars(env, class_name, &isCopy);

    return converted;
  }
}

// Called when a breakpoint is hit.
void JNICALL BreakpointCallback(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thread, jmethodID method, jlocation location) {
  jobject the_object;
  int err = (*jvmti_env)->GetLocalObject(jvmti_env, thread, 0, 1, &the_object);

  // Call the "getPath" method.
  jclass clazz = (*jni)->GetObjectClass(jni, the_object);
  jmethodID tostring_method = (*jni)->GetMethodID(jni, clazz, "toString", "()Ljava/lang/String;");

  jstring result = (jstring) (*jni)->CallObjectMethod(jni, the_object, tostring_method);
  jboolean isCopy;
  const char *converted = (*jni)->GetStringUTFChars(jni, result, &isCopy);

  // Find which method we're in.
  int i = 0;
  const char *method_name = "unknown";
  for (;;) {
    if (break_methods[i] == NULL) break;

    if (method == break_methodIDs[i]) {
      method_name = break_methods[i];
    }

    i++;
  }

  printf("%s: %s\n", method_name, converted);
 
  (*jni)->ReleaseStringUTFChars(jni, result, converted);
  (*jni)->DeleteLocalRef(jni, clazz);

}

// Called when a class is loaded.
void JNICALL ClassPrepareCallback(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thread, jclass klass) {
  jclass testClass = (*jni)->FindClass(jni, CLASS_TO_WATCH);
  (*jni)->ExceptionClear(jni);

  if (registered == 1) {
    return;
  }

  if (testClass != NULL) {
    registered = 1;

    int i = 0;
    for (;;) {
      const char *method = break_methods[i];
      if (method == NULL) break;
      const char *signature = break_method_signatures[i];

      jmethodID mid = (*jni)->GetMethodID(jni, testClass, method, signature);
      break_methodIDs[i] = mid;
      if (mid != NULL) {
        assert(JVMTI_ERROR_NONE == (*jvmti_env)->SetBreakpoint(jvmti_env, mid, 0));
      } else {
        printf("Can't find the method: %s.%s%s\n", CLASS_TO_WATCH, method, signature);
      }

      i++;
    }

    (*jni)->DeleteLocalRef(jni, testClass);
  }
}


JNIEXPORT jint JNICALL 
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  printf("Loaded agent\n");

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
