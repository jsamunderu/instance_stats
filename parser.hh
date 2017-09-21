#ifndef PARSER_HH
#define PARSER_HH

#include <cstdlib>
#include <cstring>
#include <string>

namespace amazon {

enum Instance {M1, M2, M3};

const char *instance_name[] = {"M1", "M2", "M3"}; 

const int NUM_INSTANCES = sizeof(instance_name) / sizeof(instance_name[0]);

struct Data {
	int host_id;
	int nslots;
	Instance instance_type;
	bool *slot_state;
};

namespace {

int __tokens[128] = {0};

const int __states[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

const int __next[10][7] =
{
{-1, 1, 1, 1, 1, -1, -1},
{-1, 1, 1, 1, 1, -1, 2},
{-1, -1, -1, -1, -1, 3, -1},
{-1, -1, 4, 4, 4, -1, -1},
{-1, -1, -1, -1, -1, -1, 5},
{-1, 6, 6, 6, 6, -1, -1},
{-1, 6, 6, 6, 6, -1, 7},
{-1, 8, 8, -1, -1, -1, -1},
{-1, -1, -1, -1, -1, -1, 9},
{-1, 8, 8, -1, -1, -1, -1}
};

}

#define GET_NEXT_STATE(current_state, current_char) \
	__next[__states[(current_state)]][__tokens[(current_char) & 0xFF]]

#define SET_TOKEN(tkn, id) __tokens[(tkn)] = (id)

/* a state machine driven parser */

struct Parser {
public:
	enum ErrorCode {
		SUCCESS = 0,
		BAD_CHARACTER = -1,
		SLOTS_INCONSISTENT = -2
	};

	static void init()
	{
		SET_TOKEN('0', 1);
		SET_TOKEN('1', 2);
		SET_TOKEN('2', 3);
		SET_TOKEN('3', 3);
		SET_TOKEN('4', 4);
		SET_TOKEN('5', 4);
		SET_TOKEN('6', 4);
		SET_TOKEN('7', 4);
		SET_TOKEN('8', 4);
		SET_TOKEN('9', 4);
		SET_TOKEN('M', 5);
		SET_TOKEN(',', 6);
	}

	static ErrorCode parse(std::string &line, amazon::Data &data)
	{
		int state = 0, prev_marker = 0, counter = 0;
		char instance_type[3];
		for (unsigned i = 0; i < line.size(); i++) {
			if (::isspace(line.at(i)))
				continue;
			switch (state = GET_NEXT_STATE(state, line.at(i))) {
			case 2: {
				std::string host_id;
				host_id.assign(line.c_str() + prev_marker,
					i - prev_marker);
				data.host_id = ::atoi(host_id.c_str());
				prev_marker = i + 1;
				break;
			}
			case 5:
				instance_type[0] = line.at(prev_marker);
				instance_type[1] = line.at(prev_marker + 1);
				instance_type[2] = '\0';
				if (::strcmp(instance_type,
						amazon::instance_name[amazon::Instance::M1]) == 0)
					data.instance_type = amazon::Instance::M1;
				else if (::strcmp(instance_type,
						amazon::instance_name[amazon::Instance::M2]) == 0)
					data.instance_type = amazon::Instance::M2;
				else if (::strcmp(instance_type,
						amazon::instance_name[amazon::Instance::M3]) == 0)
					data.instance_type = amazon::Instance::M3;
				prev_marker = i + 1;
				break;
			case 7: {
				std::string nslots;
				nslots.assign(line.c_str() + prev_marker,
					i - prev_marker);
				data.nslots = ::atoi(nslots.c_str());
				data.slot_state = new bool[data.nslots];
				prev_marker = i + 1;
				break;
			}
			case 8:
				if (line.at(i + 1) != ',') {
					data.slot_state[counter++] = line.at(i) & 1;
					prev_marker = i + 1;
				}
				break;
			case 9: {
				data.slot_state[counter++] = line.at(i - 1) & 1;
				prev_marker = i + 1;
				break;
			}
			case -1:
				return ErrorCode::BAD_CHARACTER;
			}
		}
		if (data.nslots != counter) {
			delete[] data.slot_state;
			return ErrorCode::SLOTS_INCONSISTENT;
		}
		return ErrorCode::SUCCESS;
	}
};

}

#endif
