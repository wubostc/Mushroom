/**
 *    > Author:            UncP
 *    > Mail:         770778010@qq.com
 *    > Github:    https://www.github.com/UncP/Mushroom
 *    > Created Time:  2017-03-22 18:39:17
**/

#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <iomanip>
#include <thread>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "../src/db.hpp"

int main(int argc, char **argv)
{
	using namespace Mushroom;

	const int total = (argc == 2) ? atoi(argv[1]) : 1;
	const int key_len = 16;
	const std::vector<std::string> files = {
		std::string("../data/16_2500000_0_random"),
		std::string("../data/16_2500000_1_random"),
		std::string("../data/16_2500000_2_random"),
		std::string("../data/16_2500000_3_random")
	};

	KeySlice::SetStringFormat([](const KeySlice *key, uint8_t len) {
		return std::string(key->Data(), len) + "\n";
	});

	std::cerr << "\ntotal: " << total << "\n";
	auto beg = std::chrono::high_resolution_clock::now();
	int child = 4;
	pid_t id[child];
	for (int i = 0; i < child; ++i) {
		if (!(id[i] = fork())) {
			MushroomDB db("../mushroom", key_len);
			char tmp[BTreePage::PageByte + key_len] = {0};
			KeySlice *key = (KeySlice *)tmp;
			int fd = open(files[i].c_str(), O_RDONLY);
			assert(fd > 0);
			char buf[8192];
			int curr = 0, ptr = 0, count = 0;
			bool flag = true;
			for (; (ptr = pread(fd, buf, 8192, curr)) > 0 && flag; curr += ptr) {
				while (--ptr && buf[ptr] != '\n' && buf[ptr] != '\0') buf[ptr] = '\0';
				if (ptr) buf[ptr++] = '\0';
				else break;
				for (int i = 0; i < ptr;) {
					int j = 0;
					char *tmp = buf + i;
					for (; buf[i] != '\n' && buf[i] != '\0'; ++i, ++j) ;
					tmp[j] = '\0';
					memcpy(key->Data(), tmp, key_len);
					db.Put(key);
					if (++count == total) {
						flag = false;
						break;
					}
					++i;
				}
			}
			close(fd);
			db.Close();
			exit(0);
		}
	}
	int status;
	for (int i = 0; i < child; ++i)
		waitpid(id[i], &status, 0);
	auto end = std::chrono::high_resolution_clock::now();
	auto Time = std::chrono::duration<double, std::ratio<1>>(end - beg).count();
	std::cerr << "put time: " << std::setw(8) << Time << "  s\n";

	MushroomDB db("../mushroom", key_len);
	beg = std::chrono::high_resolution_clock::now();
	bool flag = true;
	std::vector<std::thread> vec2;
	for (int i = 0; i != child; ++i)
		vec2.push_back(std::thread([&, i] {
			if (!db.FindSingle(files[i].c_str(), total))
				__sync_bool_compare_and_swap(&flag, true, false);
		}));
	for (auto &e : vec2)
		e.join();
	end = std::chrono::high_resolution_clock::now();
	Time = std::chrono::duration<double, std::ratio<1>>(end - beg).count();
	std::cerr << "get time: " << std::setw(8) << Time << "  s\n";
	if (!flag)
		std::cerr << "\033[31mFail :(\033[0m\n";
	else
		std::cerr << "\033[32mSuccess :)\033[0m\n";
	db.Close();
	return 0;
}