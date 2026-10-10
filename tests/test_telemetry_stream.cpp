#include <cstdio>
#include <fstream>
#include <string>

#include "dataplane/DataPlaneSim.h"
#include "telemetry/TelemetryStream.h"
#include "test_main.h"

using namespace fabric;

void RunTests() {
  const std::string output_path = "/tmp/gpu_fabric_telemetry_decay_test.csv";
  {
    telemetry::TelemetryStream telemetry(output_path);
    dataplane::DataPlaneSim data_plane;

    data_plane.SendOnLink(7, 8.0, 10000, 1000, 0, telemetry);
    data_plane.SendOnLink(7, 8.0, 10000, 1000, 0, telemetry);

    CHECK(telemetry.Snapshot(7, 0).queue_depth_bytes == 1000);
    CHECK(telemetry.Snapshot(7, 1).queue_depth_bytes == 0);

    telemetry.FlushTick(0);
    telemetry.FlushTick(1);
  }

  std::ifstream input(output_path);
  std::string header;
  std::string first_tick;
  std::string second_tick;
  std::getline(input, header);
  std::getline(input, first_tick);
  std::getline(input, second_tick);
  input.close();
  std::remove(output_path.c_str());

  CHECK(header ==
        "time_us,link_id,queue_depth_bytes,queue_capacity_bytes,bytes_sent,"
        "packets_sent,packets_dropped");
  CHECK(first_tick == "0,7,1000,10000,2000,2,0");
  CHECK(second_tick == "1,7,0,10000,2000,2,0");
}

TEST_MAIN(RunTests)
