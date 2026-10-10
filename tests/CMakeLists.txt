# Lightweight unit tests -- no external test framework dependency, each
# test file is its own executable with a hand-rolled assert-based runner
# (see tests/test_main.h). Keeps the skeleton buildable offline without
# fetching Catch2/GTest.

add_executable(test_event_queue test_event_queue.cpp)
target_link_libraries(test_event_queue PRIVATE fabric_sim_core)
add_test(NAME test_event_queue COMMAND test_event_queue)

add_executable(test_clos_topology test_clos_topology.cpp)
target_link_libraries(test_clos_topology PRIVATE fabric_sim_core)
add_test(NAME test_clos_topology COMMAND test_clos_topology)

add_executable(test_congestion_strategies test_congestion_strategies.cpp)
target_link_libraries(test_congestion_strategies PRIVATE fabric_sim_core)
add_test(NAME test_congestion_strategies COMMAND test_congestion_strategies)

add_executable(test_incast_detector test_incast_detector.cpp)
target_link_libraries(test_incast_detector PRIVATE fabric_sim_core)
add_test(NAME test_incast_detector COMMAND test_incast_detector)

add_executable(test_telemetry_stream test_telemetry_stream.cpp)
target_link_libraries(test_telemetry_stream PRIVATE fabric_sim_core)
add_test(NAME test_telemetry_stream COMMAND test_telemetry_stream)

add_executable(test_many_to_one_pattern test_many_to_one_pattern.cpp)
target_link_libraries(test_many_to_one_pattern PRIVATE fabric_sim_core)
add_test(NAME test_many_to_one_pattern COMMAND test_many_to_one_pattern)
