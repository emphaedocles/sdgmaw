#pragma once
#include <string>
#include <vector>
class DPSTrack
{
private:
	
	std::vector<std::pair<float, double>> _dpsEntries; // pair of (time stamp, damage)
	int _maxEntries = 1000; // max number of entries to keep for averaging
public:
	DPSTrack() = default;
	DPSTrack(const std::string& name) : Name(name) {}
	std::string Name; 
	void AddEntry(float time, double damage);
	float GetAverageDps(float timeSpan);
	bool Empty() { return Name.empty(); }
};

