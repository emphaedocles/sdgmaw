#pragma once
#include <string>
class CharacterDetails
{
public:
	
	std::string Name;
	std::string Class;
	unsigned int MaxHealth;
	int Health;
	unsigned int MaxMana;
	unsigned int Mana;
	unsigned int ManaPoolMax;
	unsigned int Level;	
	unsigned int HealthRegen;
	unsigned int ManaRegen;
	unsigned int AC;
	std::string StatusFx;
	std::string MeleeRating;
	std::string RangedRating;
	std::string SpellRating;
	std::string Vitality;
};

