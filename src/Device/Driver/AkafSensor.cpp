/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2012 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Device/Driver/AkafSensor.hpp"
#include "Device/Driver.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/InputLine.hpp"
#include "Atmosphere/Temperature.hpp"
#include "NMEA/InputLine.hpp"
#include "Units/System.hpp"

#include <stdlib.h>

class AkafDevice : public AbstractDevice {

public:
//  AkafDevice();

  virtual bool ParseNMEA(const char *line, struct NMEAInfo &info) override;
};

static bool
ParsePAFGA(NMEAInputLine &line, NMEAInfo &info)
{
  // e.g.
  // $PAFGA,962.14,hPa,962,m,20.01,C*


	fixed value;

	if (line.ReadChecked(value)) {  /* read baro pressure */
		AtmosphericPressure pressure = AtmosphericPressure::HectoPascal(fixed(value));
    info.ProvideStaticPressure(pressure);
	}
	
	line.Skip(); /* cancel unit of baro pressure */
	
	// Outside Air Temperature (?C) (i.e. +15.2)
  if (line.ReadChecked(value)) {
    info.temperature = CelsiusToKelvin(value);
    info.temperature_available = true;
  }
	
  return true;
}

static bool
ParsePAFGB(NMEAInputLine &line, NMEAInfo &info)
{
  // e.g.
  // $PAFGB,+948.67,+23.55,+967.77*CRC
  // $PAFGB,static_pressure,dynamic_pressure,tek_pressure*CRC

	fixed value;

	// Static pressure
	if (line.ReadChecked(value)) {  /* read baro pressure */
		AtmosphericPressure static_pressure = AtmosphericPressure::HectoPascal(fixed(value));
    info.ProvideStaticPressure(static_pressure);
	}

	// dynamic pressure
	if (line.ReadChecked(value)) {
		AtmosphericPressure dyn_pressure = AtmosphericPressure::Pascal(fixed(value*100));
    info.ProvideDynamicPressure(dyn_pressure);
	}
	
	// True air speed [km/h] (i.e. 183)
   // info.ProvideTrueAirspeed(Units::ToSysUnit(111, Unit::KILOMETER_PER_HOUR));
  return true;
}

/*gcc_pure
static inline
fixed ComputeNoncompVario(const fixed pressure, const fixed d_pressure)
{
  static constexpr fixed FACTOR(-2260.389548275485);
  static constexpr fixed EXPONENT(-0.8097374740609689);
  return fixed(FACTOR * pow(pressure, EXPONENT) * d_pressure);
}*/

bool
AkafDevice::ParseNMEA(const char *String, NMEAInfo &info)
{ 
	// check if Checksum in sentence is correct
	//if (!VerifyNMEAChecksum(String))
	//return false;
  
	NMEAInputLine line(String);
  char type[16];
  line.Read(type, 16);
	
	if (StringIsEqual(type, "$PAFGA"))
    return ParsePAFGA(line, info);
		
	if (StringIsEqual(type, "$PAFGB"))
    return ParsePAFGB(line, info);

  return false;
}


static Device *
AkafCreateOnPort(const DeviceConfig &config, Port &com_port)
{
  return new AkafDevice();
}

const struct DeviceRegister akaf_driver = {
  _T("Akaf_sensor"),
  _T("AKAF Sensors"),
  0,
  AkafCreateOnPort,
};
