# Install script for directory: /home/swgadmin/Desktop/SWGFlurry/MMOCoreORB

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/utils/engine3/MMOEngine/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/utils/googletest-release-1.10.0/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/conf/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/terrain/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/templates/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/pathfinding/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/server/db/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/server/status/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/server/web/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/server/ping/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/server/login/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/server/chat/cmake_install.cmake")
  include("/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/src/client/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/swgadmin/Desktop/SWGFlurry/MMOCoreORB/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
