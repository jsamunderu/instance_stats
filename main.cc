#include <iostream>
#include <fstream>
#include <sstream>
#include "parser.hh"
#include "workqueue.hh"

const int NUMBER_OF_THREADS = 4;
const int QUEUE_SIZE = 16;
const char *output_file = "Statistics.txt";

int main(int argc, char *argv[])
{
	std::mutex mtx;
	std::condition_variable cond;
	amazon::GlobalStats stats[amazon::NUM_INSTANCES];
	amazon::Workqueue::Event event;
	amazon::Data data;

	if (argc != 2) {
		std::cout<< "You should specify the input file - filename" << std::endl;
		std::cout<< "usage: zonestats filename" << std::endl;
		return -1;
	}

	std::ifstream infile(argv[1]);
	if (! infile.is_open()) {
		std::cout<< "Failed to open file" << std::endl;
		return -1;
	}
	if (! infile.good()) {
		std::cout<< "File is bad" << std::endl;
		return -1;
	}

	amazon::Workqueue workqueue(QUEUE_SIZE, NUMBER_OF_THREADS, cond, stats);

	amazon::Parser::init();

	int line_num = 1;
	std::string line;
	while (getline(infile, line)) {
		if (line.size() < 1)
			continue;
		std::cout<<line<<std::endl;
		switch (amazon::Parser::parse(line, data)) {
		case amazon::Parser::ErrorCode::BAD_CHARACTER:
			std::cout<< "ERROR! line: " << line_num
				<< " in the wrong format" << std::endl;
			break;
		case amazon::Parser::ErrorCode::SLOTS_INCONSISTENT:
			std::cout<< "ERROR! line: "
				<< line_num
				<< " the specified slots do not correspond to those available"
				<< std::endl;
			break;
		case amazon::Parser::ErrorCode::SUCCESS:
			std::cout<< "hostId: " <<data.host_id << " instanceType: "
				<< amazon::instance_name[data.instance_type] << " slots: "
				<< data.nslots << " ";
			for (int i = 0; i < data.nslots; i++)
				std::cout << " " << data.slot_state[i];
			std::cout<<std::endl;
			event.type = amazon::Workqueue::ev_type::WORK;
			event.data = data;
			workqueue.push(event);
			break;
		}
		std::cout<<std::endl;
		++line_num;
	}

	event.type = amazon::Workqueue::ev_type::STOP;
	for (int i = 0; i < NUMBER_OF_THREADS; i++)
		workqueue.push(event);

	std::ofstream outfile(output_file);
	if (! outfile.is_open()) {
		std::cout<< "Failed to open file" << std::endl;
		return -1;
	}

	if (! outfile.good()) {
		std::cout<< "File is bad" << std::endl;
		return -1;
	}

	/* wait for workers to finish */
	{
		std::unique_lock<std::mutex> lock(mtx);
		while (workqueue.active() > 0)
			cond.wait(lock);
	}

	{
		int m1_empty = stats[amazon::Instance::M1].empty.load();
		int m2_empty = stats[amazon::Instance::M2].empty.load();
		int m3_empty = stats[amazon::Instance::M3].empty.load();

		std::stringstream empty;
		empty << "EMPTY: M1=<" << m1_empty << ">;M2=<" << m2_empty << ">;M3=<" << m3_empty << ">;";
		outfile << empty.str() <<std::endl;
	}

	{
		int m1_full = stats[amazon::Instance::M1].full.load();
		int m2_full = stats[amazon::Instance::M2].full.load();
		int m3_full = stats[amazon::Instance::M3].full.load();

		std::stringstream full;
		full << "FULL: M1=<" << m1_full << ">;M2=<" << m2_full << ">;M3=<" << m3_full << ">;";
		outfile << full.str() <<std::endl;
	}

	{
		int m1_most_filled_first = stats[amazon::Instance::M1].most_filled.first.load();
		int m1_most_filled_second = stats[amazon::Instance::M1].most_filled.second.load();
		int m2_most_filled_first = stats[amazon::Instance::M2].most_filled.first.load();
		int m2_most_filled_second = stats[amazon::Instance::M2].most_filled.second.load();
		int m3_most_filled_first = stats[amazon::Instance::M3].most_filled.first.load();
		int m3_most_filled_second = stats[amazon::Instance::M3].most_filled.first.load();

		std::stringstream most_filled;
		most_filled <<"MOST FILLED: M1=<" << m1_most_filled_first
			<< "," << m1_most_filled_second << ">;M2=<"
			<< m2_most_filled_first << "," 
			<< m2_most_filled_second << ">;M3=<"
			<< m3_most_filled_first << "," << m3_most_filled_second << ">;";
		outfile << most_filled.str();
	}

	return EXIT_SUCCESS;
}
