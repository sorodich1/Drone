cmake_minimum_required(VERSION 3.10)
project(Drone)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Указываем директории для заголовочных файлов
include_directories(include)
include_directories("${CMAKE_SOURCE_DIR}/cpp-httplib")

# Сначала проверим стандартные места
find_library(WIRINGPI_LIB 
    NAMES wiringPi
    PATHS 
        /usr/lib
        /usr/local/lib
        /usr/lib/arm-linux-gnueabihf
        ${CMAKE_SOURCE_DIR}/libs  # Локальная директория проекта
        ${CMAKE_SOURCE_DIR}/wiringPi
        ${CMAKE_SOURCE_DIR}/WiringPi
)

# Также ищем заголовочные файлы
find_path(WIRINGPI_INCLUDE_DIR
    NAMES wiringPi.h
    PATHS
        /usr/include
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/include  # Локальная директория проекта
        ${CMAKE_SOURCE_DIR}/wiringPi
        ${CMAKE_SOURCE_DIR}/WiringPi
)

if(WIRINGPI_LIB AND WIRINGPI_INCLUDE_DIR)
    message(STATUS "Found wiringPi library: ${WIRINGPI_LIB}")
    message(STATUS "Found wiringPi headers: ${WIRINGPI_INCLUDE_DIR}")
    include_directories(${WIRINGPI_INCLUDE_DIR})
else()
    message(WARNING "wiringPi not found, attempting to build without GPIO support")
    # Можно добавить определение для компиляции без GPIO
    add_definitions(-DNO_GPIO_SUPPORT)
endif()

find_package(MAVSDK REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Создаем исполняемый файл
add_executable(Drone 
    src/main.cpp
    src/MissionController.cpp
    src/TakeoffLandController.cpp
    src/PositionSender.cpp
    src/CameraCorrectionController.cpp
    src/ActuatorController.cpp
    src/LEDController.cpp
)

# Свяжите MAVSDK с вашим проектом
target_link_libraries(Drone MAVSDK::mavsdk)

# Дополнительные библиотеки
target_link_libraries(Drone 
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Добавляем wiringPi если нашли
if(WIRINGPI_LIB)
    target_link_libraries(Drone ${WIRINGPI_LIB})
else()
    message(WARNING "Building without wiringPi - GPIO functions will not work")
endif()