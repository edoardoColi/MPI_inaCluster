#include <iostream>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <stop_token>
#include <random>
#include <thread>

int main(int argc, char *argv[]) {
	int nprod = 4;
	int ncons = 3;
	if (argc != 1 && argc != 3) {
		std::printf("use: %s #prod #cons\n", argv[0]);
		return -1;
	}
	if (argc > 1) {
		nprod = std::stol(argv[1]);
		ncons = std::stol(argv[2]);
	}
	
    std::vector<std::jthread> producers;
    std::vector<std::jthread> consumers;

    std::stop_source stopSrc;
    std::stop_token stoken = stopSrc.get_token();

	std::mutex mtx;
	std::condition_variable_any cv;
	std::deque<uint64_t> dataq;
	
	auto random = [](const int &min, const int &max) {
		// better to have different per-thread seeds....
		thread_local std::mt19937   
			generator(std::hash<std::thread::id>()(std::this_thread::get_id()));
		std::uniform_int_distribution<int> distribution(0,100);
		return distribution(generator);
	};		
	auto producer = [&](const std::stop_token &stoken, int id) {	   
		int i = 0;
		while (!stoken.stop_requested()) {
			{
				std::lock_guard<std::mutex> lock(mtx);
				dataq.push_back(i++);
			}
			cv.notify_one();
			// do something
			std::this_thread::sleep_for(std::chrono::milliseconds(random(0,100))); 
		}
		std::printf("Producer%d exits\n",id);
	};
	auto consumer = [&](const std::stop_token &stoken, int id) {
		while (!stoken.stop_requested() || !dataq.empty()) {
			std::unique_lock<std::mutex> lock(mtx);
			cv.wait(lock, stoken, [&dataq] { return !dataq.empty(); });
			if (!dataq.empty()) {
				uint64_t data = dataq.front();
				dataq.pop_front();
				//std::printf("Consumer%d, data=%lu\n", id, data);
				lock.unlock();
			}
			// do something
			std::this_thread::sleep_for(std::chrono::milliseconds(random(0,100)));
		}
		std::printf("Consumer%d exits\n",id);
	};
	
	for (int i = 0; i < ncons; ++i)
        consumers.emplace_back(consumer, stoken, i);	
    for (int i = 0; i < nprod; ++i)
        producers.emplace_back(producer, stoken, i);

    // waits some time before stopping the computation
    std::this_thread::sleep_for(std::chrono::seconds(3));	
	std::cout << "Stopping computation\n";
    stopSrc.request_stop();

	
	for (auto& thread : consumers) thread.join();
	for (auto& thread : producers) thread.join();

    return 0;
}
