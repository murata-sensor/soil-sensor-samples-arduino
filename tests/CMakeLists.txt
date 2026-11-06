cmake_minimum_required(VERSION 3.16)

project(slt5006_host_tests LANGUAGES CXX)

enable_testing()

add_executable(
  slt5006_host_tests
  ../src/SLT5006.cpp
  test_slt5006.cpp
)

target_include_directories(
  slt5006_host_tests
  PRIVATE
    fake_arduino
    ../src
)
target_compile_features(slt5006_host_tests PRIVATE cxx_std_11)

if(MSVC)
  target_compile_options(slt5006_host_tests PRIVATE /W4 /WX)
else()
  target_compile_options(
    slt5006_host_tests
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror
  )
endif()

option(SLT5006_ENABLE_SANITIZERS "Enable host-test sanitizers" OFF)
if(SLT5006_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    slt5006_host_tests PRIVATE -fno-omit-frame-pointer -fsanitize=address,undefined
  )
  target_link_options(slt5006_host_tests PRIVATE -fsanitize=address,undefined)
endif()

add_test(NAME slt5006_host_tests COMMAND slt5006_host_tests)

add_executable(logger_number_parser_tests test_logger_number_parser.cpp)
target_compile_features(logger_number_parser_tests PRIVATE cxx_std_11)
if(MSVC)
  target_compile_options(logger_number_parser_tests PRIVATE /W4 /WX)
else()
  target_compile_options(
    logger_number_parser_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
  )
endif()
add_test(NAME logger_number_parser_tests COMMAND logger_number_parser_tests)

add_executable(upload_policy_tests test_upload_policy.cpp)
target_compile_features(upload_policy_tests PRIVATE cxx_std_11)
if(MSVC)
  target_compile_options(upload_policy_tests PRIVATE /W4 /WX)
else()
  target_compile_options(
    upload_policy_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
  )
endif()
add_test(NAME upload_policy_tests COMMAND upload_policy_tests)

add_executable(queue_order_tests test_queue_order.cpp)
target_compile_features(queue_order_tests PRIVATE cxx_std_11)
if(MSVC)
  target_compile_options(queue_order_tests PRIVATE /W4 /WX)
else()
  target_compile_options(
    queue_order_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
  )
endif()
add_test(NAME queue_order_tests COMMAND queue_order_tests)

set(ARDUINOJSON_INCLUDE_DIR "" CACHE PATH "ArduinoJson src directory for logger tests")
if(ARDUINOJSON_INCLUDE_DIR)
  add_executable(logger_config_tests test_logger_config.cpp)
  target_include_directories(
    logger_config_tests PRIVATE fake_logger "${ARDUINOJSON_INCLUDE_DIR}"
  )
  target_compile_features(logger_config_tests PRIVATE cxx_std_11)
  if(MSVC)
    target_compile_options(logger_config_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(logger_config_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME logger_config_tests COMMAND logger_config_tests)

  add_executable(cloud_record_tests test_cloud_record.cpp)
  target_include_directories(
    cloud_record_tests PRIVATE fake_logger "${ARDUINOJSON_INCLUDE_DIR}"
  )
  target_compile_features(cloud_record_tests PRIVATE cxx_std_11)
  if(MSVC)
    target_compile_options(cloud_record_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(cloud_record_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME cloud_record_tests COMMAND cloud_record_tests)
endif()
