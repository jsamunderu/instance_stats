#ifndef QUEUE_HH
#define QUEUE_HH

#include <mutex>
#include <condition_variable>

namespace amazon {

template<class T>
class Ring {
public:
	Ring(unsigned size)
	{
		this->len = npw2(size);
		this->mask = len - 1;
		this->begin = 0;
		this->end = 0;
		this->count = 0;
		this->ring = new T[this->len];
	}
	~Ring()
	{
		delete[] ring;
	}
	bool push(T &val)
	{
		if (count == len)
			return false;
		ring[end] = val;
		end = (end + 1) & mask;
		++count;
		return true;
	}
	bool pop(T &val)
	{
		if (count == 0)
			return false;
		val = ring[begin];
		begin = (begin + 1) & mask;
		--count;
		return true;
	}
	unsigned size()
	{
		return count;
	}
private:
	unsigned len;
	unsigned mask;
	unsigned begin;
	unsigned end;
	unsigned count;
	T *ring;

	/* next power of 2 from Hackers Delight, by Henry S. Warren */
	unsigned npw2(unsigned n)
	{
		n--;
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n |= n >> 16;
		n++;
		return n;
	}
};

template<class T>
class Queue {
public:
	Queue(unsigned len) : insert_waiters(0), remove_waiters(0)
	{
		ring = new Ring<T>(len);
	}
	~Queue() { delete ring; }
	bool push(T &val)
	{
		std::unique_lock<std::mutex> lock(mtx);
		if (! ring->push(val)) {
			++insert_waiters;
			do
				insert_cond.wait(lock);
			while (! ring->push(val));
			--insert_waiters;
		}
		if (remove_waiters > 0)
			remove_cond.notify_one();
		return true; /* since we wait till we get some */
	}
	bool try_push(T &val)
	{
		std::unique_lock<std::mutex> lock(mtx);
		if (ring->push(val)) {
			if (remove_waiters > 0)
				remove_cond.notify_one();
			return true;
		}
		return false;
	}
	bool try_pop(T &val)
	{
		std::unique_lock<std::mutex> lock(mtx);
		if (ring->pop(val)) {
			if (insert_waiters > 0)
				insert_cond.notify_one();
			return true;
		}
		return false;
	}
	bool pop(T &val)
	{
		std::unique_lock<std::mutex> lock(mtx);
		if (! ring->pop(val)) {
			++remove_waiters;
			do
				remove_cond.wait(lock);
			while (! ring->pop(val));
			--remove_waiters;
		}
		if (insert_waiters > 0)
			insert_cond.notify_one();
		return true; /* since we wait till we get some */
	}
	unsigned size()
	{
		std::unique_lock<std::mutex> lock(mtx);
		return ring->size();
	}
private:
	unsigned len;
	mutable std::mutex mtx;
	std::condition_variable insert_cond;
	std::condition_variable remove_cond;
	unsigned insert_waiters;
	unsigned remove_waiters;
	Ring<T> *ring;
};

}

#endif
