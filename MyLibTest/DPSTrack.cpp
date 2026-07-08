#include "pch.h"
#include "DPSTrack.h"

void DPSTrack::AddEntry(float time, double damage)
{
	_dpsEntries.emplace_back(time, damage);
	while (_dpsEntries.size() > _maxEntries)
	{
		_dpsEntries.erase(_dpsEntries.begin());
	}
}

float DPSTrack::GetAverageDps(float timeSpan)
{
	float maxDelta = 2560;//if subsequent entries are more than this apart, we consider them as separate "sessions" and stop averaging (e.g. player died and respawned)
	if (timeSpan <= 0.0f) return 0.0f;
	float timeSum = 0;
	float dpsSum = 0.0f;
	int index = (int)_dpsEntries.size() - 1;
	int c = 0;
	float timeFirst;
	float timeEnd;
	float timePrev;
	//any entry with a 0 timestamp is ignored, as it is likely an invalid entry (e.g. from a previous game session or uninitialized)
	//or is the "initalized" value
	while (c < timeSpan && index >= 0 && !_dpsEntries.empty())
	{
		auto& entry = _dpsEntries[index];
		if (entry.first > 0) {
			if (c == 0) 
			{
				timeFirst = entry.first;
				timePrev = timeFirst;
			}
			timeEnd = entry.first;
			if (abs(timeEnd - timePrev) < maxDelta)
			{
				dpsSum += entry.second;
				timePrev = timeEnd;
				c++;
			}
			else
			{
				break;
			}
		}
		index--;
	}
	timeSum = abs(timeFirst - timeEnd);
	if (timeSum > 0.0f)
		return (dpsSum / (timeSum / 256 ));
	return 0.0f;
}
