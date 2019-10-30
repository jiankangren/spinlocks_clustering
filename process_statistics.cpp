#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>

using namespace std;

int main(int argc, char** argv) {

	if (argc != 2) {
		cout << "Usage: ./program statistics_file" << endl;
		return -1;
	}

	ifstream ifs(argv[1]);
	if (!ifs) {
		cout << "Cannot open statistics file !" << endl;
		return -1;
	}

	string line;
	unsigned resource_id, event_num;

	getline(ifs, line);
	istringstream iss(line);
	iss >> resource_id >> event_num;
	//cout << "Resource id: " << resource_id << ". Number of events: " << event_num << endl;

	char events[event_num];
	if (!ifs.read(events, event_num)) {
		cout << "Cannot read events" << endl;
	}
	
	// Map the number of requests in the resource queue to number of times that happens
	map<unsigned, unsigned> counter;
	for (int i=0; i<event_num; i++) {
		counter[events[i]]++;
	}

	unsigned num_nonzero_events = 0;
	for (map<unsigned, unsigned>::iterator it=counter.begin(); it!=counter.end(); it++) {

		// Abort events for zero request in queue
		if (it->first == 0) {
			continue;
		}

		//cout << it->first << "\t" << it->second << endl;

		num_nonzero_events += it->second;
	}


	// Map the number of requests in queue to the percent of its apperance
	map<unsigned, float> percentages;
	for (map<unsigned, unsigned>::iterator it=counter.begin(); it != counter.end(); it++) {
		if (it->first == 0) {
			continue;
		}

		float percent = 100.0*(it->second)/num_nonzero_events;
		percentages[it->first] = percent;

		cout << it->first << "\t" << percentages[it->first] << endl;
	}

	cout << endl;


#ifdef DEBUG

	float total_percents = 0;
	for (map<unsigned, float>::iterator it=percentages.begin(); it!=percentages.end(); it++) {
		total_percents += it->second;
	}

	cout << "Total percents: " << total_percents << endl;
#endif


	return 0;
}
