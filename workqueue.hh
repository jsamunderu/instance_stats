#ifndef WORKQUEUE_HH
#define WORKQUQUE_HH

#include <atomic>
#include <thread>
#include <condition_variable>
#include <memory>
#include <tuple>
#include <utility>

#include "queue.hh"
#include "parser.hh"

namespace amazon {

struct GlobalStats {
	std::atomic<int> empty;
	std::atomic<int> full;
	std::pair<std::atomic<int> , std::atomic<int> >most_filled;
	GlobalStats()
	{
		empty.store(0);
		full.store(0);
		most_filled.first.store(0);
		most_filled.second.store(0);
	}
};

class Workqueue {
public:
	enum ev_type {STOP, WORK};

	struct Event {
		ev_type type;
		amazon::Data data;
	};

	Workqueue(unsigned size, unsigned nworkers, std::condition_variable &cond,
		amazon::GlobalStats *all_stats)
		: size(size), nworkers(nworkers), cond(cond), all_stats(all_stats)
	{
		active_workers.store(nworkers);
		workers = new std::thread*[nworkers];
		workqueue = new Queue<Event>(size);
		for (unsigned i = 0; i < nworkers; i++)
			workers[i] = new std::thread(std::bind(&Workqueue::run, this));
	}
	void push(Event &ev)
	{
		workqueue->push(ev);
	}
	int active()
	{
		return active_workers.load();
	}
	~Workqueue()
	{
		Event ev;
		ev.type = ev_type::STOP;
		for (unsigned i = active_workers.load(); i > 0; i--)
			workqueue->push(ev);
		for (unsigned i = 0; i < nworkers; i++)
			workers[i]->join();
		for (unsigned i = 0; i < nworkers; i++)
			delete workers[i];
		delete workers;
		delete workqueue;
	}

private:
	struct PerThreadStats {
		int empty;
		int full;
		std::pair<int, int> most_filled;
		PerThreadStats() : empty(0), full(0)
		{
			most_filled.first = 0;
			most_filled.second = 0;
		}
	};

	unsigned size;
	unsigned nworkers;
	std::condition_variable &cond;
	std::thread **workers;
	Queue<Event> *workqueue;
	amazon::GlobalStats *all_stats;
	std::atomic<int> active_workers;
	void run()
	{
		PerThreadStats stats[amazon::NUM_INSTANCES];
		for (;;) {
			Event ev;
			workqueue->pop(ev);
			switch (ev.type) {
			case ev_type::WORK:
				do_work(ev.data, stats);
				delete[] ev.data.slot_state;
				break;
			case ev_type::STOP:
				accum_stats(stats);
				active_workers.fetch_sub(1);
				if (active_workers.load() == 0)
					cond.notify_one();
				return;
			}
		}
	}
	void do_work(amazon::Data &data, PerThreadStats stats[])
	{
		int empty = 0, full = 0;
		for (int i = 0; i < data.nslots; i++)
			switch(data.slot_state[i]) {
			case 0:
				++empty;
				++stats[data.instance_type].most_filled.first; 
				break;
			case 1:
				++full;
				++stats[data.instance_type].most_filled.second; 
			}
		if (empty == data.nslots)
			++stats[data.instance_type].empty;
		if (full == data.nslots)
			++stats[data.instance_type].full;
	}
	void accum_stats(PerThreadStats stats[])
	{
		all_stats[amazon::Instance::M1].empty.fetch_add(stats[amazon::Instance::M1].empty);
		all_stats[amazon::Instance::M1].full.fetch_add(stats[amazon::Instance::M1].full);
		all_stats[amazon::Instance::M1].most_filled.first.fetch_add(stats[amazon::Instance::M1].most_filled.first);
		all_stats[amazon::Instance::M1].most_filled.second.fetch_add(stats[amazon::Instance::M1].most_filled.second);

		all_stats[amazon::Instance::M2].empty.fetch_add(stats[amazon::Instance::M2].empty);
		all_stats[amazon::Instance::M2].full.fetch_add(stats[amazon::Instance::M2].full);
		all_stats[amazon::Instance::M2].most_filled.first.fetch_add(stats[amazon::Instance::M2].most_filled.first);
		all_stats[amazon::Instance::M2].most_filled.second.fetch_add(stats[amazon::Instance::M2].most_filled.second);

		all_stats[amazon::Instance::M3].empty.fetch_add(stats[amazon::Instance::M3].empty);
		all_stats[amazon::Instance::M3].full.fetch_add(stats[amazon::Instance::M3].full);
		all_stats[amazon::Instance::M3].most_filled.first.fetch_add(stats[amazon::Instance::M3].most_filled.first);
		all_stats[amazon::Instance::M3].most_filled.second.fetch_add(stats[amazon::Instance::M3].most_filled.second);
	}
};

}

#endif
