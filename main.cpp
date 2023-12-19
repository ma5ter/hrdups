#include <iostream>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <map>
#include <vector>
#include <openssl/sha.h>

std::map<std::string, std::pair<size_t, std::vector<std::string>>> map;

void traverse(const std::filesystem::path& path) {
	for (const auto & entry : std::filesystem::directory_iterator(path)) {
		const auto& entry_path = entry.path();
		if (!entry.is_symlink()) {
			if (entry.is_directory()) {
				traverse(entry_path);
			}
			if (entry.is_regular_file()) {
				std::ostringstream os;
				std::ifstream file(entry_path, std::ifstream::ate | std::ifstream::binary);
				const auto size = file.tellg();

				// skip empty files
				if (!size) {
					file.close();
					continue;
				}

				file.seekg(std::ifstream::beg);

				if (not file.good()) {
					os << "Cannot open \"" << path << "\": " << std::strerror(errno) << ".";
					throw std::runtime_error(os.str());
				}

				constexpr const std::size_t buffer_size { 1 << 12 };
				char buffer[buffer_size];

				unsigned char hash[SHA256_DIGEST_LENGTH] = { 0 };

				SHA256_CTX ctx;
				SHA256_Init(&ctx);

				while (file.good()) {
					file.read(buffer, buffer_size);
					SHA256_Update(&ctx, buffer, file.gcount());
				}
				file.close();

				SHA256_Final(hash, &ctx);

				os << std::hex << std::setfill('0');
				os << size;

				for (unsigned char i: hash) {
					os << std::setw(2) << static_cast<unsigned int>(i);
				}

				auto &group = map[os.str()];
				group.first = size;
				group.second.emplace_back(entry_path.string());

				// std::cout << entry_path << os.str() << std::endl;
			}
		}
	}
}

int main(int argc, char** argv) {

	std::string path("./");

	for (argv++, argc--; argc; argc--) {
		auto option = *argv++;
		#define set_option(name, conversion) if (0 == strcmp("--" # name, option)) { if (argc == 1) { throw std::invalid_argument("missing argument for --" # name ); } name = conversion(*argv++); argc--; }
		#define set_bool_option(name) if (0 == strcmp("--" # name, option)) { (name) = true; }
		set_option(path, std::string)
		else {
			throw std::invalid_argument("unknown option " + std::string(option));
		}
		#undef set_option
	}

	std::cout << "Building hash map..." << std::endl;
	try {
		traverse(path);
	}
	catch (const std::exception & e) {
		std::cerr << e.what();
	}

	std::cout << "Hardlinking..." << std::endl;
	size_t saved = 0;
	long number = 0;

	for (const auto & [hash, pair] : map) {
		const auto size = pair.first;
		const std::string *base = nullptr;
		bool duplicate = false;
		for (const auto & file : pair.second) {
			if (!base) {
				base = &file;
				continue;
			}

			if (!duplicate) {
				duplicate = true;
				number++;
				std::cout << "Group " << number << ":" << std::endl << "\t" << *base << std::endl;
			}
			std::cout << "\t" << file << std::endl;

			if (0 != std::remove(file.c_str())) {
				std::ostringstream os;
				os << "Cannot delete file \"" << file << "\": " << std::strerror(errno) << ".";
				throw std::runtime_error(os.str());
			}

			std::error_code ec;
			std::filesystem::create_hard_link(std::filesystem::path(base->c_str()), std::filesystem::path(file.c_str()), ec);
			if (ec.value() != 0) {
				std::ostringstream os;
				os << "Cannot create hardlink for \"" << *base << " as " << file << "\": " << ec.message() << ".";
				throw std::runtime_error(os.str());
			}

			saved += size;
		}
	}

	std::cout << "Done!" << std::endl << "Saved " << std::fixed << std::setprecision(2) << (double)saved / (1024 * 1024) << "MiB" << std::endl;
	return 0;
}
