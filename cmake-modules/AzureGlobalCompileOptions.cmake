# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.
#
# cspell: words genex

if(MSVC)
  if(WARNINGS_AS_ERRORS)
    set(WARNINGS_AS_ERRORS_FLAG "/WX")
  endif()

  # https://stackoverflow.com/questions/58708772/cmake-project-in-visual-studio-gives-flag-override-warnings-command-line-warnin
  string(REGEX REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REGEX REPLACE "/W3" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

  #https://stackoverflow.com/questions/37527946/warning-unreferenced-inline-function-has-been-removed
  add_compile_options(/permissive- /W4 ${WARNINGS_AS_ERRORS_FLAG} /wd5031 /wd4668 /wd4820 /wd4255 /wd4710)

  #https://learn.microsoft.com/cpp/build/reference/zc-cplusplus?view=msvc-170
  add_compile_options(/Zc:__cplusplus)

  # NOTE: Static analysis will slow building time considerably and it is run during CI gates.
  # It is better to turn in on to debug errors reported by CI than have it ON all the time. 
  if (DEFINED ENV{AZURE_ENABLE_STATIC_ANALYSIS})
    add_compile_options(/analyze)
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(WARNINGS_AS_ERRORS)
    set(WARNINGS_AS_ERRORS_FLAG "-Werror")
  endif()

  add_compile_options(-fno-operator-names -Wold-style-cast -Xclang -Wall -Wextra -pedantic  ${WARNINGS_AS_ERRORS_FLAG} -Wdocumentation -Wdocumentation-unknown-command -Wcast-qual)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(WARNINGS_AS_ERRORS)
    set(WARNINGS_AS_ERRORS_FLAG "-Werror")
  endif()
  # GCC objects to -fno-operator-names, and -Wold-style-cast for C code. So use a genex to limit flags per language.
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-operator-names$<SEMICOLON>-Wold-style-cast$<SEMICOLON>-Wall$<SEMICOLON>-Wextra$<SEMICOLON>-pedantic$<SEMICOLON>${WARNINGS_AS_ERRORS_FLAG}>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wall$<SEMICOLON>-Wextra$<SEMICOLON>-pedantic$<SEMICOLON>${WARNINGS_AS_ERRORS_FLAG}>)
else()
  if(WARNINGS_AS_ERRORS)
    set(WARNINGS_AS_ERRORS_FLAG "-Werror")
  endif()
  add_compile_options(-fno-operator-names -Wold-style-cast -Wall -Wextra -pedantic  ${WARNINGS_AS_ERRORS_FLAG})
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
