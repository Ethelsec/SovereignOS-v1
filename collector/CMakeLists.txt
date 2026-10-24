cmake_minimum_required(VERSION 3.20)
project(collector_telemetry_platform VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
option(TP_ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TP_ENABLE_TESTS "Build unit tests" ON)

set(TP_SOURCES
    analyzer.c anomaly_detector.c buffer.c canonical.c classifier.c cli.c config.c correlator.c dashboard.c
    decoder.c encoder.c exporter.c file_reader.c file_writer.c filter.c filesystem_monitor.c
    health.c ingress.c logger.c memory_pool.c metrics.c network_monitor.c normalizer.c otlp_client.c
    parser.c platform.c process_tracker.c protobuf_serializer.c resource_monitor.c ring_buffer.c
    scheduler.c statistics.c storage.c syscall_monitor.c telemetry.c terminal_ui.c timeline.c utilities.c
)

add_library(collector_core STATIC ${TP_SOURCES})
target_compile_definitions(collector_core PUBLIC _GNU_SOURCE)
target_compile_options(collector_core PRIVATE -Wall -Wextra)
if(TP_ENABLE_WARNINGS_AS_ERRORS)
    target_compile_options(collector_core PRIVATE -Werror)
endif()
target_link_libraries(collector_core PUBLIC pthread)

find_package(ZLIB)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBBPF libbpf)
    pkg_check_modules(PROTOBUFC protobuf-c)
endif()
if(LIBBPF_FOUND)
    target_compile_definitions(collector_core PUBLIC TP_HAVE_LIBBPF=1)
    target_include_directories(collector_core PUBLIC ${LIBBPF_INCLUDE_DIRS})
    target_link_libraries(collector_core PUBLIC ${LIBBPF_LIBRARIES})
endif()
if(PROTOBUFC_FOUND)
    target_compile_definitions(collector_core PUBLIC TP_HAVE_PROTOBUF_C=1)
    target_include_directories(collector_core PUBLIC ${PROTOBUFC_INCLUDE_DIRS})
    target_link_libraries(collector_core PUBLIC ${PROTOBUFC_LIBRARIES})
endif()
if(ZLIB_FOUND)
    target_compile_definitions(collector_core PUBLIC TP_HAVE_ZLIB=1)
    target_link_libraries(collector_core PUBLIC ZLIB::ZLIB)
endif()

add_executable(collector main.c)
target_link_libraries(collector PRIVATE collector_core)
install(TARGETS collector collector_core RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
install(FILES README.md INGEST.md MEMBER_A_REPORT.md CONFIGURATION.md TELEMETRY_FORMAT.md ARCHITECTURE.md DESIGN.md DESTINATION share/collector)

include(CTest)
if(TP_ENABLE_TESTS)
    enable_testing()
    set(TP_TESTS
        test_buffer test_parser test_storage test_exporter test_correlator test_dashboard
        test_configuration test_memory_pool test_utilities test_ingest_pipeline
        test_canonical test_ingress
    )
    foreach(test_name ${TP_TESTS})
        add_executable(${test_name} ${test_name}.c)
        target_compile_definitions(${test_name} PRIVATE TP_TEST_STANDALONE _GNU_SOURCE)
        target_compile_options(${test_name} PRIVATE -Wall -Wextra)
        if(TP_ENABLE_WARNINGS_AS_ERRORS)
            target_compile_options(${test_name} PRIVATE -Werror)
        endif()
        target_link_libraries(${test_name} PRIVATE collector_core)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

set(CPACK_PACKAGE_NAME "collector-telemetry-platform")
set(CPACK_PACKAGE_VENDOR "Collector Project")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "TGZ")
include(CPack)
