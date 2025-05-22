#pragma once
#include <Arduino.h>
#include <tuple>

class OnChainState {
  public:
  OnChainState(String flowRestAccess);
  std::tuple<unsigned long, bool> get_latest_sealed_block();
  std::tuple<int64_t, bool> get_led_state_at_block(unsigned long block); // explicit on/off logic
  String getURL();

  static const String Cadence_Script_Retrieving_Led_State;

  private:
  std::tuple<String, bool> extractPayloadFromResponse(String rawResponse);
  std::tuple<int64_t, bool> parsePayload(String rawResponse);

  // behavioral parameters are lifetime-constants (provided at construction)
  const String url;
  const String postBody;

  // behavioral parameters are lifetime-constants (provided at construction) dynamic state parameters
  // none
};
