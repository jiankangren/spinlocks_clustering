#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

using namespace std;

int main(int argc, char** argv) {
	// Max core number is 47 (cores is numbered from 0-47)
	const unsigned max_core = 47;

	if (argc != 2) {
		cout << "Usage: ./program rtps_file" << endl;
		return -1;
	}

	string file_path(argv[1]);
	ifstream ifs(argv[1]);
	if (!ifs.good()) {
		cout << "Open file failed !" << endl;
		return -1;
	}
	
	// Read all lines from .rtps file and store in reverse order
	vector<string> lines;
	string line;
	while (getline(ifs, line)) {
		if (line.empty())
			continue;

		lines.insert(lines.begin(), line);
	}

	string first_line = lines[lines.size()-1];
	unsigned sched_status;
	istringstream ss(first_line);
	ss >> sched_status;
	if (sched_status == 2) {
		cout << "Unschedulable task set: " << file_path << endl;
		return 1;
	}

	string last_line = lines[0];
	istringstream iss(last_line);
	unsigned first_core, last_core;
	iss >> first_core >> last_core;

	// If the last core assigned is larger than the maximum available core,
	// print this task set number
	if (last_core > max_core) {
		reverse(file_path.begin(), file_path.end());
		size_t pos = file_path.find('/');
		string taskset_num_str;

		if (pos == string::npos) {
			// Not found '/' character, so just find it from the .rtps file name
			reverse(file_path.begin(), file_path.end());
			//taskset_num_str = file_path.substr(7, file_path.length()-17);
			cout << "Invalid task set: " << file_path << endl;
		} else {
			// Found a '/' character, get the .rtps file name first
			string file_name = file_path.substr(0, pos);
			reverse(file_name.begin(), file_name.end());
			//taskset_num_str = file_name.substr(7, file_name.length()-17);
			cout << "Invalid task set: " << file_name << endl;
		}

		//unsigned taskset_num;
		//istringstream ss(taskset_num_str);
		//ss >> taskset_num;
		//cout << "Invalid task set: " << taskset_num << endl;
	}

	return 0;
}
