/*
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
// Boardwalk: missing include
#include <string.h>

#include "log.h"

#include "utils.h"

// PojavLancher: fixme: are these wrong?
#define FULL_VERSION "1.9.0-internal"
#define DOT_VERSION "1.9"

typedef jint JNI_CreateJavaVM_func(JavaVM **pvm, void **penv, void *args);

typedef jint JLI_Launch_func(int argc, char ** argv, /* main argc, argc */
        int jargc, const char** jargv,          /* java args */
        int appclassc, const char** appclassv,  /* app classpath */
        const char* fullversion,                /* full version defined */
        const char* dotversion,                 /* dot version defined */
        const char* pname,                      /* program name */
        const char* lname,                      /* launcher name */
        jboolean javaargs,                      /* JAVA_ARGS */
        jboolean cpwildcard,                    /* classpath wildcard*/
        jboolean javaw,                         /* windows-only javaw */
        jint ergo                               /* ergonomics class policy */
);

static void logArgs(int argc, char** argv) {
/* BlockLauncher: disable logging
    int i;
    
    for (i = 0; i < argc; i++) {
        LOGD("arg[%d]: %s", i, argv[i]);
    }
*/
}

JNIEXPORT jint JNICALL Java_com_oracle_dalvik_VMLauncher_createLaunchMainJVM(JNIEnv *env, jclass clazz, jobjectArray vmArgArr, jstring mainClassStr, jobjectArray mainArgArr) {
    void *libjvm = dlopen("libjvm.so", RTLD_NOW + RTLD_GLOBAL);
    if (libjvm == NULL) {
        LOGE("dlopen failed to open libjvm.so (dlerror %s).", dlerror());
        return -1;
    }
    
    JNI_CreateJavaVM_func *jl_JNI_CreateJavaVM = (JNI_CreateJavaVM_func *) dlsym(libjvm, "JNI_CreateJavaVM");
        if (jl_JNI_CreateJavaVM == NULL) {
        LOGE("dlsym failed to get JNI_CreateJavaVM (dlerror %s).", dlerror());
        return -1;
    }
    
    int vm_argc = (*env)->GetArrayLength(env, vmArgArr);
    char **vm_argv = convert_to_char_array(env, vmArgArr);
    
    int main_argc = (*env)->GetArrayLength(env, mainArgArr);
    char **main_argv = convert_to_char_array(env, mainArgArr);
    
    JavaVMInitArgs vm_args;
    JavaVMOption options[vm_argc];
    for (int i = 0; i < vm_argc; i++) {
        options[i].optionString = vm_argv[i];
    }
    vm_args.version = JNI_VERSION_1_6;
    vm_args.options = options;
    vm_args.nOptions = vm_argc;
    vm_args.ignoreUnrecognized = JNI_FALSE;
    
    jint res = (jint) jl_JNI_CreateJavaVM(&runtimeJavaVMPtr, (void**)&runtimeJNIEnvPtr_JRE, &vm_args);
    // delete options;
    
    char *main_class_c = (*env)->GetStringUTFChars(env, mainClassStr, 0);
    
    jclass mainClass = (*runtimeJNIEnvPtr_JRE)->FindClass(runtimeJNIEnvPtr_JRE, main_class_c);
    jmethodID mainMethod = (*runtimeJNIEnvPtr_JRE)->GetStaticMethodID(runtimeJNIEnvPtr_JRE, mainClass, "main", "([Ljava/lang/String;)V");

    // Need recreate jobjectArray to make JNIEnv is 'runtimeJNIEnvPtr_JRE'.
    jobjectArray runtime_main_argv = convert_from_char_array(runtimeJNIEnvPtr_JRE, main_argv, main_argc);
    (*runtimeJNIEnvPtr_JRE)->CallStaticVoidMethod(runtimeJNIEnvPtr_JRE, mainClass, mainMethod, runtime_main_argv);
    
    (*env)->ReleaseStringUTFChars(env, mainClassStr, main_class_c);
    free_char_array(env, mainArgArr, main_argv);
    free_char_array(env, vmArgArr, vm_argv);
    
    return res;
}

static jint launchJVM(int argc, char** argv) {
    logArgs(argc, argv);

   void* libjli = dlopen("libjli.so", RTLD_LAZY | RTLD_GLOBAL);
   
   // Boardwalk: silence
   // LOGD("JLI lib = %x", (int)libjli);
   if (NULL == libjli) {
       LOGE("JLI lib = NULL: %s", dlerror());
       return -1;
   }
   LOGD("Found JLI lib");

   JLI_Launch_func *pJLI_Launch =
          (JLI_Launch_func *)dlsym(libjli, "JLI_Launch");
    // Boardwalk: silence
    // LOGD("JLI_Launch = 0x%x", *(int*)&pJLI_Launch);

   if (NULL == pJLI_Launch) {
       LOGE("JLI_Launch = NULL");
       return -1;
   }

   LOGD("Calling JLI_Launch");

   return pJLI_Launch(argc, argv, 
       0, NULL, 0, NULL, FULL_VERSION,
       DOT_VERSION, *argv, *argv, /* "java", "openjdk", */
       JNI_FALSE, JNI_TRUE, JNI_FALSE, 0);
}

/*
 * Class:     com_oracle_embedded_launcher_VMLauncher
 * Method:    launchJVM
 * Signature: ([Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_oracle_dalvik_VMLauncher_launchJVM(JNIEnv *env, jclass clazz, jobjectArray argsArray) {
   jint res = 0;
   // int i;

    // Save dalvik JNIEnv pointer for JVM launch thread
    dalvikJNIEnvPtr_ANDROID = env;

    if (argsArray == NULL) {
        LOGE("Args array null, returning");
        //handle error
        return 0;
    }

    int argc = (*env)->GetArrayLength(env, argsArray);
    char **argv = convert_to_char_array(env, argsArray);
    
    LOGD("Done processing args");

    res = launchJVM(argc, argv);

    LOGD("Freeing args");
    free_char_array(env, argsArray, argv);
    
    LOGD("Free done");
   
    return res;
}
static int pfd[2];
static pthread_t logger;
static const char* tag = "jrelog";

static void *logger_thread() {
    ssize_t  rsize;
    char buf[512];
    while((rsize = read(pfd[0], buf, sizeof(buf)-1)) > 0) {
        if(buf[rsize-1]=='\n') {
            rsize=rsize-1;
        }
        buf[rsize]=0x00;
        __android_log_write(ANDROID_LOG_SILENT,tag,buf);
    }
}

JNIEXPORT void JNICALL
Java_net_kdt_pojavlaunch_JREUtils_redirectLogcat(JNIEnv *env, jclass clazz) {
    // TODO: implement redirectLogcat()
    setvbuf(stdout, 0, _IOLBF, 0); // make stdout line-buffered
    setvbuf(stderr, 0, _IONBF, 0); // make stderr unbuffered

    /* create the pipe and redirect stdout and stderr */
    pipe(pfd);
    dup2(pfd[1], 1);
    dup2(pfd[1], 2);

    /* spawn the logging thread */
    if(pthread_create(&logger, 0, logger_thread, 0) == -1) {
        __android_log_write(ANDROID_LOG_ERROR,tag,"Error while spawning logging thread. JRE output won't be logged.");
    }

    pthread_detach(logger);
    __android_log_write(ANDROID_LOG_INFO,tag,"Starting logging STDIO as jrelog:V");
}
