# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/karan-gandhi/esp/esp-idf/components/bootloader/subproject"
  "/home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_mitm/build/bootloader"
  "/home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_mitm/build/bootloader-prefix"
  "/home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_mitm/build/bootloader-prefix/tmp"
  "/home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_mitm/build/bootloader-prefix/src/bootloader-stamp"
  "/home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_mitm/build/bootloader-prefix/src"
  "/home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_mitm/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_mitm/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_mitm/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
