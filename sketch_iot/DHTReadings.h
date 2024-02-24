#ifndef DHTREADINGS_H
#define DHTREADINGS_H 1

#include <cmath>
#include <DHT.h>

class DHTReadings {
public:
  inline DHTReadings(float humidity, float cTemperature, float fTemperature)
    :_humidity{ humidity }, _cTemperature{ cTemperature }, _fTemperature{ fTemperature }
  { }

  DHTReadings(const DHTReadings&) = default;

  virtual inline ~DHTReadings() { }

  DHTReadings& operator=(const DHTReadings&) = default;

  inline float humidity() const {
    return this->_humidity;
  }

  inline float cTemperature() const {
    return this->_cTemperature;
  }

  inline float fTemperature() const {
    return this->_fTemperature;
  }

  inline float computeHeatIndex(DHT *const dht, bool forFahrenheit = false) {
    float temp = (forFahrenheit) ? this-> _fTemperature : this->_cTemperature;
    return dht->computeHeatIndex(temp, this->_humidity, forFahrenheit);
  }

  inline bool validate() {
    return (
      std::isnan(this->_humidity) ||
      std::isnan(this->_cTemperature) ||
      std::isnan(this->_fTemperature)
    );
  }

private:
  float _humidity;
  float _cTemperature;
  float _fTemperature;
};

#endif // DHTREADINGS_H