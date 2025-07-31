# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/laizhiquan/esp/v5.4.2/esp-idf/components/bootloader/subproject"
  "/Users/laizhiquan/coding/esp/PWM/build/bootloader"
  "/Users/laizhiquan/coding/esp/PWM/build/bootloader-prefix"
  "/Users/laizhiquan/coding/esp/PWM/build/bootloader-prefix/tmp"
  "/Users/laizhiquan/coding/esp/PWM/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/laizhiquan/coding/esp/PWM/build/bootloader-prefix/src"
  "/Users/laizhiquan/coding/esp/PWM/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/laizhiquan/coding/esp/PWM/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/laizhiquan/coding/esp/PWM/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
