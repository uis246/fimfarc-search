#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <charconv>
#include <stdint.h>

namespace fs = std::filesystem;
using tp = std::pair<uint32_t, uint32_t>;
//using tp = uint64_t;

int main(int argc, char *argv[]) {
	//1. Load(mmap?) all groups
	if(argc != 2) {
		std::cout << "Usage: " << argv[0] << " BINTREE_PATH" << std::endl;
		return -1;
	}
	const fs::path root(argv[1]);
	std::vector<tp> map;
	for(const std::filesystem::directory_entry &i : fs::recursive_directory_iterator(root, fs::directory_options::follow_directory_symlink)) {
		if(!i.is_regular_file())
			continue;
		const fs::path &fpath = i.path();
		const std::uintmax_t fsize = i.file_size();
		const std::string filename = fpath.filename();
		uint32_t folder_id = 0;
		if(std::from_chars(filename.c_str(), filename.c_str() + filename.length(), folder_id).ptr != filename.c_str() + filename.length())
			throw std::exception{};
		if(folder_id == 0)
			throw std::exception{};
		std::vector<uint32_t> buf(fsize/sizeof(uint32_t));
		std::ifstream file(fpath, std::ios::in | std::ios::binary);
		file.read(reinterpret_cast<char*>(buf.data()), fsize/sizeof(uint32_t)*sizeof(uint32_t));
		std::transform(buf.begin(), buf.end(), std::back_inserter(map), [folder_id](uint32_t id){ return tp(id, folder_id); });
	}
	//2. Transpose to map of vectors(?)
	std::sort(map.begin(), map.end(), [](const tp &a, const tp &b){ return a.first < b.first || (a.first == b.first && a.second < b.second); });
	//std::sort(map.begin(), map.end(), [](const tp &a, const tp &b){ return a < b; });
	//3. Serialize?
	std::ofstream out("./groupassoc.bin", std::ios::binary | std::ios::binary);
	for(const tp &i : map) {
		uint32_t buf[2] = {i.first, i.second};
		out.write(reinterpret_cast<char*>(buf), sizeof(buf));
	}
}
