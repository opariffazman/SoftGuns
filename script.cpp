#include "script.h"
#include "keyboard.h"

#include <fstream>
#include <sstream>
#include <ctime>
#include <map>

using namespace std;

// logging functions
const char* const LOG_FILE = "SoftGuns.log";

ofstream file;

void initializeLogger()
{
	file.open(LOG_FILE, std::ios_base::out);
	file.close();
}

bool isFileExists(const char* fileName)
{
	std::ifstream infile(fileName);
	return infile.good();
}

void writeLog(const char* msg)
{
	struct tm newtime;
	time_t now = time(0);
	localtime_s(&newtime, &now);
	stringstream text;

	file.open(LOG_FILE, ios_base::app);
	if (file.is_open())
	{
		text << "[" << newtime.tm_mday << "/" << newtime.tm_mon + 1 << "/" << newtime.tm_year + 1900 << " " << newtime.tm_hour << ":" << newtime.tm_min << ":" << newtime.tm_sec << "] " << msg;
		file << text.str().c_str() << "\n";
		file.close();
	}
}

void initialize()
{
	initializeLogger();
	writeLog("### SoftGuns Mod by opsedar ###");
	(isFileExists(".\\SoftGuns.ini")) ? writeLog("### SoftWeapon.ini found ###") : writeLog("### SoftGuns.ini not found ###");
}

// math functions
int bRound(float x)
{
	return BUILTIN::ROUND(x);
}

int bCeil(float x)
{
	return BUILTIN::CEIL(x);
}

int bFloor(float x)
{
	return BUILTIN::FLOOR(x);
}

float toFloat(int x)
{
	return BUILTIN::TO_FLOAT(x);
}

// hash function
Hash key(const char* text)
{
	return MISC::GET_HASH_KEY(text);
}

// ui function
void showSubtitle(const char* text)
{
	UILOG::_UILOG_SET_CACHED_OBJECTIVE((const char*)MISC::_CREATE_VAR_STRING(10, "LITERAL_STRING", text));
	UILOG::_UILOG_PRINT_CACHED_OBJECTIVE();
	UILOG::_UILOG_CLEAR_CACHED_OBJECTIVE();
}

float celciusToFarenheit(float temperature) // using exactly the same formula based on the decompiled scripts
{
	return ((temperature * 1.8f) + 32.0f);
}

float getSurroundingTemperature()
{
	Vector3 playerPos = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), true, true);
	float temperature = MISC::_GET_TEMPERATURE_AT_COORDS(playerPos.x, playerPos.y, playerPos.z); // this function return in celcius

	return (MISC::_SHOULD_USE_METRIC_TEMPERATURE()) ? temperature : celciusToFarenheit(temperature);
}

int getGameTimer()
{
	return MISC::GET_GAME_TIMER();
}

bool isPlayerOutside()
{
	return (INTERIOR::GET_INTERIOR_FROM_ENTITY(PLAYER::PLAYER_PED_ID()) == 0) ? true : false;
}

bool isPlayerPlaying()
{
	return PLAYER::IS_PLAYER_PLAYING(PLAYER::PLAYER_ID()) && ENTITY::DOES_ENTITY_EXIST(PLAYER::PLAYER_PED_ID());
}

bool isSubmerged()
{
	return ENTITY::IS_ENTITY_IN_WATER(PLAYER::PLAYER_PED_ID())
		|| (ENTITY::GET_ENTITY_SUBMERGED_LEVEL(PLAYER::PLAYER_PED_ID()) > 0.0f);
}

bool isRaining()
{
	return MISC::GET_RAIN_LEVEL() > 0.0f;
}

bool isSnowing()
{
	return MISC::GET_SNOW_LEVEL() > 0.0f;
}

bool isWeaponDegradable(Hash weapon)
{
	const char* degradableWeapons[] = {
		"GROUP_PISTOL",
		"GROUP_REPEATER",
		"GROUP_REVOLVER",
		"GROUP_RIFLE",
		"GROUP_SHOTGUN",
		"GROUP_SNIPER"
	};

	for (const char* degradableWeapon : degradableWeapons)
	{
		if (WEAPON::GET_WEAPONTYPE_GROUP(weapon) == key(degradableWeapon))
		{
			return true;
			break;
		}
	}

	return false;
}

void setWeaponDegradation(Object weaponObject, float level)
{
	WEAPON::_SET_WEAPON_DEGRADATION(weaponObject, level);
	WEAPON::_SET_WEAPON_DAMAGE(weaponObject, level, 0);
	WEAPON::_SET_WEAPON_DIRT(weaponObject, level, 0);
	WEAPON::_SET_WEAPON_SOOT(weaponObject, level, 0);
}

void main()
{
	bool enableLogging = GetPrivateProfileInt("DEBUG", "ENABLE_LOGGING", 0, ".\\SoftGuns.ini");
	if(enableLogging) initialize();
	bool shootDegrade = GetPrivateProfileInt("MODIFIER", "SHOOT_DEGRADE", 1, ".\\SoftGuns.ini");
	bool timeDegrade = GetPrivateProfileInt("MODIFIER", "TIME_DEGRADE", 1, ".\\SoftGuns.ini");
	bool temperatureDegrade = GetPrivateProfileInt("MODIFIER", "TEMPERATURE_DEGRADE", 1, ".\\SoftGuns.ini");
	bool rainDegrade= GetPrivateProfileInt("MODIFIER", "RAIN_DEGRADE", 1, ".\\SoftGuns.ini");
	bool snowDegrade = GetPrivateProfileInt("MODIFIER", "SNOW_DEGRADE", 1, ".\\SoftGuns.ini");
	bool submergedDegrade = GetPrivateProfileInt("MODIFIER", "SUBMERGED_DEGRADE", 1, ".\\SoftGuns.ini");
	bool randomDegrade = GetPrivateProfileInt("MODIFIER", "RANDOM_DEGRADE", 1, ".\\SoftGuns.ini");

	int randomMin = GetPrivateProfileInt("RANGE", "RANDOM_MIN", 99, ".\\SoftGuns.ini");
	int randomMax = GetPrivateProfileInt("RANGE", "RANDOM_MAX", 100, ".\\SoftGuns.ini");

	float temperatureRate = (float)GetPrivateProfileInt("RATE", "TEMPERATURE_RATE", 20, ".\\SoftGuns.ini") / 100.0f;
	float rainRate = (float)GetPrivateProfileInt("RATE", "RAIN_RATE", 40, ".\\SoftGuns.ini") / 100.0f;
	float snowRate = (float)GetPrivateProfileInt("RATE", "SNOW_RATE", 60, ".\\SoftGuns.ini") / 100.0f;
	float submergedRate = (float)GetPrivateProfileInt("RATE", "SUBMERGED_RATE", 80, ".\\SoftGuns.ini") / 100.0f;

	int weaponDegradationMs = GetPrivateProfileInt("TIMERS", "WEAPON_DEGRADATION", 300000, ".\\SoftGuns.ini");
	int weaponDegradationTimer = getGameTimer() + weaponDegradationMs;

	map<const char*, float> initialDegradationMap;
	map<const char*, float> degradationRateMap;

	while (true)
	{
		Ped playerPed = PLAYER::PLAYER_PED_ID();
		Player playerID = PLAYER::PLAYER_ID();

		const int weaponAttachPoints[] = { 0, 1, 2, 3, 9, 10 }; // all weapon attachment points

		for (const int& weaponAttachPoint : weaponAttachPoints) // store first weapon equipped on body initial degradation value
		{
			Hash weaponHash;
			if (WEAPON::GET_CURRENT_PED_WEAPON(playerPed, &weaponHash, false, weaponAttachPoint, true))
			{
				if (isWeaponDegradable(weaponHash))
				{
					const char* weaponName = WEAPON::_GET_WEAPON_NAME(weaponHash);
					if (!initialDegradationMap.count(weaponName)) // if weapon initial condition haven't been stored yet in map
					{
						Entity weaponEntity = WEAPON::GET_CURRENT_PED_WEAPON_ENTITY_INDEX(playerPed, weaponAttachPoint);
						Object weaponObject = ENTITY::GET_OBJECT_INDEX_FROM_ENTITY_INDEX(weaponEntity);
						initialDegradationMap[weaponName] = WEAPON::_GET_WEAPON_DEGRADATION(weaponObject);
						stringstream text;
						text << "hooked first entry of [" << weaponName << "] initial degradation: " << initialDegradationMap[weaponName];
						writeLog(text.str().c_str());
					}
				}
			}
		}

		if (PED::IS_PED_SHOOTING(playerPed)) // calculate the rate of degradation first instance of player shooting with the weapon
		{
			Hash primaryHand;
			if (WEAPON::GET_CURRENT_PED_WEAPON(playerPed, &primaryHand, false, 0, true))
			{
				if (isWeaponDegradable(primaryHand)) // only if weapon on hand degradable
				{
					const char* primaryHandName = WEAPON::_GET_WEAPON_NAME(primaryHand);
					if (initialDegradationMap.count(primaryHandName)) 
					{
						Entity primaryHandEntity = WEAPON::GET_CURRENT_PED_WEAPON_ENTITY_INDEX(playerPed, 0);
						Object primaryHandObject = ENTITY::GET_OBJECT_INDEX_FROM_ENTITY_INDEX(primaryHandEntity);

						if (!degradationRateMap.count(primaryHandName)) // only update if not exist yet
						{
							float primaryHandDegradation = WEAPON::_GET_WEAPON_DEGRADATION(primaryHandObject); 
							
							if (primaryHandDegradation - initialDegradationMap[primaryHandName] != 0.0f) // and if there is a differences
							{
								degradationRateMap[primaryHandName] = primaryHandDegradation - initialDegradationMap[primaryHandName];
								stringstream text;
								text << "hooked first degradation of [" << primaryHandName << "] degradation rate: " << degradationRateMap[primaryHandName];
								writeLog(text.str().c_str());
							}
						}
					}
				}
			}

			Hash secondaryHand;
			if (WEAPON::GET_CURRENT_PED_WEAPON(playerPed, &secondaryHand, false, 1, true))
			{
				if (isWeaponDegradable(secondaryHand)) // only if weapon on hand degradable
				{
					const char* secondaryHandName = WEAPON::_GET_WEAPON_NAME(secondaryHand);
					if (initialDegradationMap.count(secondaryHandName))
					{
						Entity secondaryHandEntity = WEAPON::GET_CURRENT_PED_WEAPON_ENTITY_INDEX(playerPed, 1);
						Object secondaryHandObject = ENTITY::GET_OBJECT_INDEX_FROM_ENTITY_INDEX(secondaryHandEntity);

						if (!degradationRateMap.count(secondaryHandName)) // only update if not exist yet
						{
							float secondaryHandDegradation = WEAPON::_GET_WEAPON_DEGRADATION(secondaryHandObject);

							if (secondaryHandDegradation - initialDegradationMap[secondaryHandName] != 0.0f) // because dual weild
							{
								degradationRateMap[secondaryHandName] = secondaryHandDegradation - initialDegradationMap[secondaryHandName];
								stringstream text;
								text << "hooked first degradation of [" << secondaryHandName << "] degradation rate: " << degradationRateMap[secondaryHandName];
								writeLog(text.str().c_str());
							}
						}
					}
				}
			}
		}

		int f1{ VK_F1 };
		if (IsKeyJustUp(f1))
		{
			for (auto const& x : degradationRateMap)
			{
				stringstream text;
				text << "[" << x.first << "] rate: " << x.second;
				writeLog(text.str().c_str());
			}
		}

		// START WEAPON DEGRADATION =============================================================================================================
		float pointsModifier = 1.0f;

		if (isPlayerPlaying())
		{
			if (temperatureDegrade)
			{
				bool isTemperatureModifier;

				if (getSurroundingTemperature() < 0.0f && !isTemperatureModifier)
				{
					isTemperatureModifier = true;
					stringstream text;
					text << "hooked started below 0 c/f, pointsModifier: " << pointsModifier + temperatureRate;
					writeLog(text.str().c_str());
				}
				else if (getSurroundingTemperature() > 0.0f && isTemperatureModifier)
				{
					isTemperatureModifier = false;
					writeLog("hooked stopped below 0 c/f");
				}

				if (isTemperatureModifier) pointsModifier = pointsModifier + temperatureRate;
			}

			if (isPlayerOutside() && !PED::_IS_PED_USING_SCENARIO_HASH(playerPed, key("PROP_PLAYER_SLEEP_TENT_A_FRAME"))) // for rain and snow only take effect when is outside and not in tent
			{
				if (rainDegrade)
				{
					bool isRainingModifier;

					if (isRaining() && !isRainingModifier)
					{
						isRainingModifier = true;
						stringstream text;
						text << "hooked started raining, pointsModifier: " << pointsModifier + rainRate;
						writeLog(text.str().c_str());
					}
					else if (!isRaining() && isRainingModifier)
					{
						isRainingModifier = false;
						writeLog("hooked stopped raining");
					}

					if (isRainingModifier) pointsModifier = pointsModifier + rainRate;
				}

				if (snowDegrade)
				{
					bool isSnowingModifier;

					if (isSnowing() && !isSnowingModifier)
					{
						isSnowingModifier = true;
						stringstream text;
						text << "hooked started snowing, pointsModifier: " << pointsModifier + snowRate;
						writeLog(text.str().c_str());
					}
					else if (!isSnowing() && isSnowingModifier)
					{
						isSnowingModifier = false;
						writeLog("hooked stopped snowing");
					}

					if (isSnowingModifier) pointsModifier = pointsModifier + snowRate;
				}
			}	

			if (submergedDegrade)
			{
				bool isSubmergedModifier;

				if (isSubmerged() && !isSubmergedModifier)
				{
					isSubmergedModifier = true;
					stringstream text;
					text << "hooked started submerged, pointsModifier: " << pointsModifier + submergedRate;
					writeLog(text.str().c_str());
				}
				else if (!isSubmerged() && isSubmergedModifier)
				{
					isSubmergedModifier = false;
					writeLog("hooked stopped submerged");
				}

				if (isSubmergedModifier) pointsModifier = pointsModifier + submergedRate;
			}

			if (shootDegrade) // degrade when shooting
			{
				float shootRate = (float)GetPrivateProfileInt("RATE", "SHOOT_RATE", 100, ".\\SoftGuns.ini") / 100.0f;
				pointsModifier = pointsModifier + shootRate;

				if (PED::IS_PED_SHOOTING(playerPed)) // weapon degradation when shooting
				{
					bool rngModifier;

					if (randomDegrade)
					{
						int rngNumber = MISC::GET_RANDOM_INT_IN_RANGE(randomMin, randomMax);

						if (rngNumber == 100 && !rngModifier)
						{
							rngModifier = true;
							writeLog("hooked rngModifier, setting weapon degradation to maximum");
						}
						else if (rngNumber != 100)
						{
							rngModifier = false;
						}
					}

					Hash primaryHand;
					if (WEAPON::GET_CURRENT_PED_WEAPON(playerPed, &primaryHand, false, 0, true))
					{
						if (isWeaponDegradable(primaryHand)) // only if weapon on hand degradable
						{
							const char* primaryHandName = WEAPON::_GET_WEAPON_NAME(primaryHand);
							Entity primaryHandEntity = WEAPON::GET_CURRENT_PED_WEAPON_ENTITY_INDEX(playerPed, 0);
							Object primaryHandObject = ENTITY::GET_OBJECT_INDEX_FROM_ENTITY_INDEX(primaryHandEntity);

							if (degradationRateMap.count(primaryHandName)) // only apply modifier if degradation rate already exist
							{
								float prevDegradation = WEAPON::_GET_WEAPON_DEGRADATION(primaryHandObject) - degradationRateMap[primaryHandName]; // value before game degradation takes place
								float newDegradationRate = degradationRateMap[primaryHandName] * pointsModifier; // value of degradation rate times modifier
								
								(rngModifier) ? setWeaponDegradation(primaryHandObject, 1.0f) : setWeaponDegradation(primaryHandObject, prevDegradation + newDegradationRate);

								stringstream text;
								text << "hooked degradation of [" << primaryHandName << "] newDegradationRate: " << newDegradationRate << " pointsModifier: " << pointsModifier;
								writeLog(text.str().c_str());							
							}
						}
					}

					Hash secondaryHand;
					if (WEAPON::GET_CURRENT_PED_WEAPON(playerPed, &secondaryHand, false, 1, true))
					{
						if (isWeaponDegradable(secondaryHand)) // only if weapon on hand degradable
						{
							const char* secondaryHandName = WEAPON::_GET_WEAPON_NAME(secondaryHand);

							if (degradationRateMap.count(secondaryHandName)) // only apply modifier if degradation rate already exist
							{
								Entity secondaryHandEntity = WEAPON::GET_CURRENT_PED_WEAPON_ENTITY_INDEX(playerPed, 1);
								Object secondaryHandObject = ENTITY::GET_OBJECT_INDEX_FROM_ENTITY_INDEX(secondaryHandEntity);

								float prevDegradation = WEAPON::_GET_WEAPON_DEGRADATION(secondaryHandObject) - degradationRateMap[secondaryHandName]; // value before game degradation takes place
								float newDegradationRate = degradationRateMap[secondaryHandName] * pointsModifier; // value of degradation rate times modifier
								
								(rngModifier) ? setWeaponDegradation(secondaryHandObject, 1.0f) : setWeaponDegradation(secondaryHandObject, prevDegradation + newDegradationRate);
								
								stringstream text;
								text << "hooked degradation of [" << secondaryHandName << "] newDegradationRate: " << newDegradationRate << " pointsModifier: " << pointsModifier;
								writeLog(text.str().c_str());
							}
						}
					}
				}
			}

			if (timeDegrade) // degrade over time
			{
				float timeRate = (float)GetPrivateProfileInt("RATE", "TIME_RATE", 10, ".\\SoftGuns.ini") / 100.0f;
				pointsModifier = pointsModifier + timeRate;

				if (getGameTimer() > weaponDegradationTimer) // all weapon degradation over time
				{
					const int weaponAttachPoints[] = { 0, 1, 2, 3, 9, 10 };

					for (const int& weaponAttachPoint : weaponAttachPoints)
					{
						Hash weaponHash;
						if (WEAPON::GET_CURRENT_PED_WEAPON(playerPed, &weaponHash, false, weaponAttachPoint, true))
						{
							if (isWeaponDegradable(weaponHash))
							{
								const char* weaponName = WEAPON::_GET_WEAPON_NAME(weaponHash);

								if (degradationRateMap.count(weaponName))
								{
									Entity weaponEntity = WEAPON::GET_CURRENT_PED_WEAPON_ENTITY_INDEX(playerPed, weaponAttachPoint);
									Object weaponObject = ENTITY::GET_OBJECT_INDEX_FROM_ENTITY_INDEX(weaponEntity);

									float prevDegradation = WEAPON::_GET_WEAPON_DEGRADATION(weaponObject) - degradationRateMap[weaponName]; // value before game degradation takes place
									float newDegradationRate = degradationRateMap[weaponName] * pointsModifier; // value of degradation rate times modifier
									WEAPON::_SET_WEAPON_DEGRADATION(weaponObject, prevDegradation + newDegradationRate);
									stringstream text;
									text << "hooked degradation of [" << weaponName << "] newDegradationRate: " << newDegradationRate << " after " << weaponDegradationMs << " ms";
									writeLog(text.str().c_str());
								}
							}
						}
					}

					weaponDegradationTimer = getGameTimer() + weaponDegradationMs;
				}
			}
		}
		// END WEAPON DEGRADATION ===============================================================================================================
		WAIT(0);
	}
}

void ScriptMain()
{
	srand(static_cast<int>(GetTickCount64()));
	main();
}