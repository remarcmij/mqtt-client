#include <array>
#include <utility>

// https://blog.ampow.com/lipo-voltage-chart/
const std::array<std::pair<uint32_t, uint32_t>, 21> voltage_to_percentage{
    {{4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80}, {3980, 75},
     {3950, 70},  {3910, 65}, {3870, 60}, {3850, 55}, {3840, 50}, {3820, 45},
     {3800, 40},  {3790, 35}, {3770, 30}, {3750, 25}, {3730, 20}, {3710, 15},
     {3690, 10},  {3610, 5},  {3270, 0}}};

uint32_t getBatteryCharge(uint32_t voltage) {
  for (auto entry : voltage_to_percentage) {
    if (voltage >= entry.first) {
      return entry.second;
    }
  }
  return 0;
}