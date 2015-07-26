#pragma once
#include <vector>
#include <functional>
#include <exception>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include "event_loop.h"

namespace kaiu {

/*class FileSystem {
public:
	FileSystem(AbstractEventLoop& loop);
	void read_dir(std::function<bool(std::string)> callback);
	void read_dir(size_t block_size, std::function<bool(std::vector<std::string>)> callback);

private:
	AbstractEventLoop& loop;
};*/

}
