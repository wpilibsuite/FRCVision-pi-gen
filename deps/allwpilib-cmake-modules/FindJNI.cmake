#   JNI_INCLUDE_DIRS      = the include dirs to use
#   JNI_LIBRARIES         = the libraries to use
#   JNI_FOUND             = TRUE if JNI headers and libraries were found.
#   JAVA_AWT_LIBRARY      = the path to the jawt library
#   JAVA_JVM_LIBRARY      = the path to the jvm library
#   JAVA_INCLUDE_PATH     = the include path to jni.h
#   JAVA_INCLUDE_PATH2    = the include path to jni_md.h
#   JAVA_AWT_INCLUDE_PATH = the include path to jawt.h

set(JNI_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/../02-extract/jni" "${CMAKE_SOURCE_DIR}/../02-extract/jni/linux")
set(JNI_LIBRARIES )
set(JNI_FOUND YES)
set(JAVA_AWT_LIBRARY )
set(JAVA_JVM_LIBRARY )
set(JAVA_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/../02-extract/jni")
set(JAVA_INCLUDE_PATH2 "${CMAKE_SOURCE_DIR}/../02-extract/jni/linux")
set(JAVA_AWT_INCLUDE_PATH )
