#ifndef __khrplatform_h_
#define __khrplatform_h_

#if defined(_WIN32) && !defined(__SCITECH_SNAP__)
#define KHRONOS_APICALL __declspec(dllimport)
#elif defined(__SYMBIAN32__)
#define KHRONOS_APICALL IMPORT_C
#elif defined(__ANDROID__)
#define KHRONOS_APICALL __attribute__((visibility("default")))
#else
#define KHRONOS_APICALL
#endif

#if defined(_WIN32) && !defined(__SCITECH_SNAP__)
#define KHRONOS_APIENTRY __stdcall
#else
#define KHRONOS_APIENTRY
#endif

#define KHRONOS_APIATTRIBUTES

typedef signed char khronos_int8_t;
typedef unsigned char khronos_uint8_t;
typedef signed short int khronos_int16_t;
typedef unsigned short int khronos_uint16_t;

#ifdef _WIN64
typedef signed long long int khronos_intptr_t;
typedef unsigned long long int khronos_uintptr_t;
typedef signed long long int khronos_ssize_t;
typedef unsigned long long int khronos_usize_t;
#else
typedef signed long int khronos_intptr_t;
typedef unsigned long int khronos_uintptr_t;
typedef signed long int khronos_ssize_t;
typedef unsigned long int khronos_usize_t;
#endif

typedef signed long long int khronos_int64_t;
typedef unsigned long long int khronos_uint64_t;

typedef signed int khronos_int32_t;
typedef unsigned int khronos_uint32_t;

typedef float khronos_float_t;

typedef khronos_uint64_t khronos_utime_nanoseconds_t;
typedef khronos_int64_t khronos_stime_nanoseconds_t;

#define KHRONOS_MAX_ENUM 0x7FFFFFFF

typedef enum {
  KHRONOS_FALSE = 0,
  KHRONOS_TRUE = 1,
  KHRONOS_BOOLEAN_ENUM_FORCE_SIZE = KHRONOS_MAX_ENUM
} khronos_boolean_enum_t;

#endif /* __khrplatform_h_ */
