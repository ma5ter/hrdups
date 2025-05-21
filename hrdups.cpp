#include <iostream>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <map>
#include <unistd.h>
#include <vector>
#include <openssl/sha.h>
#include <sys/stat.h>

int verbose = 0;

// data structure is:
// hash-map of size->hash-map of hash->filename
typedef std::map<size_t, std::map<std::string, std::vector<std::string> > > traverse_map_t;

/// \brief Returns the size of a given file in bytes.
/// \param[in] file_path The path to the file.
/// \description This function opens the file in binary mode, gets its size using the tellg() function, and then closes the file.
size_t fsize(const std::filesystem::path &file_path) {
	std::ifstream file(file_path, std::ifstream::ate | std::ifstream::binary);
	const auto size = file.tellg();
	file.close();
	return size;
}

/// \brief Calculates the SHA-256 hash of a given file.
/// \param[in] file_path The path to the file whose hash is to be calculated.
/// \return The calculated SHA-256 hash as a hexadecimal string.
/// \description This function reads the contents of the given file in binary mode and calculates its SHA-256 hash using the OpenSSL library's SHA256_CTX structure. The resulting hash is then converted to a hexadecimal string and returned.
/// \note The function throws a std::runtime_error if it cannot open the file.
std::string fhash(const std::filesystem::path &file_path) {
	std::ostringstream os;
	std::ifstream file(file_path, std::ifstream::binary);

	if (not file.good()) {
		os << "Cannot open \"" << file_path << "\": " << strerror(errno) << ".";
		throw std::runtime_error(os.str());
	}

	if (verbose) std::cout << "\t" << file_path.string();

	constexpr const std::size_t buffer_size{ 1 << 12 };
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

	for (unsigned char i: hash) {
		os << std::setw(2) << static_cast<unsigned int>(i);
	}

	const auto hash_str = os.str();
	if (verbose) {
		if (verbose > 1) {
			std::cout << " " << hash_str << std::endl;
		}
		else {
			std::cout << std::endl;
		}
	}
	return hash_str;
}

/// \brief Compares the owner, mode, and other attributes of two files.
/// \param[in] a The path to the first file.
/// \param[in] b The path to the second file.
/// \return True if the owner, mode, and other attributes of the two files are equal, false otherwise.
/// \details This function uses the stat() system call to retrieve the file status information for both files and compares the owner (st_uid), mode (st_mode), and other attributes (st_dev, st_ino, st_nlink, st_rdev, st_size, st_atime, st_mtime, st_ctime).
/// \note The function returns false if any of the attributes do not match.
bool compare_file_attributes(const char *a, const char *b) {
	struct stat stat1, stat2;
	if (stat(a, &stat1) != 0 || stat(b, &stat2) != 0) {
		return false;
	}
	return (stat1.st_uid == stat2.st_uid && stat1.st_gid == stat2.st_gid && stat1.st_mode == stat2.st_mode && stat1.st_dev == stat2.st_dev);
}

/// \brief Recursively traverses the given directory and its subdirectories, collecting files of the same size and calculating their hashes.
/// \param[in] path The root directory to start the traversal from.
/// \param[in/out] map A map where the keys are file sizes and the values are vectors of file paths with the same size.
void traverse(const std::filesystem::path &path, traverse_map_t &map) {
	for (const auto &entry: std::filesystem::directory_iterator(path)) {
		const auto &entry_path = entry.path();
		if (!entry.is_symlink()) {
			if (entry.is_directory()) {
				traverse(entry_path, map);
			}
			if (entry.is_regular_file()) {
				const auto size = fsize(entry_path);
				if (size) {
					auto &same_size_map = map[size];
					const auto count = same_size_map.size();
					if (count == 0) {
						// don't calculate hash for the first entry right now
						same_size_map[""].emplace_back(entry_path.string());
					}
					else {
						if (count == 1) {
							// lazy hash calculation
							auto file_names = std::move(same_size_map.begin()->second);
							same_size_map.clear();
							same_size_map[fhash(file_names[0])] = std::move(file_names);
						}
						same_size_map[fhash(entry_path)].emplace_back(entry_path.string());
					}
				}
			}
		}
	}
}

int main(int argc, char **argv) {
	bool keep = false;
	bool pretend = false;
	bool remove = false;
	std::vector<std::string> paths;

	for (argv++, argc--; argc; argc--) {
		auto option = *argv++;
		if (0 == strcmp(option, "-h") || 0 == strcmp(option, "--help")) {
			std::cout << "Hardlink (remove) duplicates, options:" << std::endl << "\t-h (--help)\tshow this help" << std::endl <<
			"\t-k (--keep)\tkeep empty folders on remove" << std::endl << "\t-p (--pretend)\tdry-run" << std::endl <<
			"\t-r (--remove)\tdon't hardlink duplicates, just remove" << std::endl <<
			"\t-v (--verbose)\texplain hashing process (repeat the option for more verbose output)" << std::endl;
			return 0;
		}
		if (0 == strcmp(option, "-k") || 0 == strcmp(option, "--keep")) {
			keep = true;
			continue;
		}
		if (0 == strcmp(option, "-p") || 0 == strcmp(option, "--pretend")) {
			pretend = true;
			continue;
		}
		if (0 == strcmp(option, "-r") || 0 == strcmp(option, "--remove")) {
			remove = true;
			continue;
		}
		if (0 == strcmp(option, "-v") || 0 == strcmp(option, "--verbose")) {
			verbose++;
			continue;
		}
		paths.emplace_back(option);
	}

	if (paths.empty()) {
		paths.emplace_back("./");
	}

	traverse_map_t map;

	try {
		std::cout << "Building hash map..." << std::endl;
		for (const auto &path: paths) {
			traverse(path, map);
		}
	}
	catch (const std::exception &e) {
		std::cerr << "Warning:" << e.what() << std::endl;
	}

	std::cout << (remove ? "Removing..." : "Hard-linking...") << std::endl;
	size_t saved = 0;
	long number = 0;

	for (const auto &[size, same_size_map]: map) {
		for (const auto &[hash, file_names]: same_size_map) {
			const auto count = file_names.size();

			// need at least a couple
			if (count < 2) continue;

			// std::sort(file_names.begin(), file_names.end(), [](const std::string &a, const std::string &b){
			//	return a < b;
			// });

			const auto &base = file_names[0];
			std::cout << "Group " << ++number << ":" << std::endl << "*\t" << base << std::endl;

			for (size_t i = 1; i < count; ++i) {
				const auto &file = file_names[i];
				std::cout << "\t" << file << std::endl;

				const char *base_str = base.c_str();
				const char * file_str = file.c_str();

				if (compare_file_attributes(base_str, file_str)) {
					if (!pretend) {
						if (0 != std::remove(file_str)) {
							std::ostringstream os;
							os << "Cannot delete file \"" << file << "\": " << std::strerror(errno) << ".";
							throw std::runtime_error(os.str());
						}

						if (remove) {
							if (!keep) {
								// check for empty folder
								const auto dir = std::filesystem::path(file).parent_path();
								if (std::filesystem::is_directory(dir)) {
									bool empty = true;
									for (const auto &entry: std::filesystem::directory_iterator(dir)) {
										empty = false;
										break;
									}
									if (empty) {
										if (0 != std::remove(dir.c_str())) {
											std::ostringstream os;
											os << "Cannot delete empty directory \"" << dir << "\": " << std::strerror(errno) << ".";
											throw std::runtime_error(os.str());
										}
										else {
											std::cout << "Empty directory removed " << dir << std::endl;
										}
									}
								}
							}
						}
						else {
							std::error_code ec;
							std::filesystem::create_hard_link(std::filesystem::path(base_str), std::filesystem::path(file_str), ec);
							if (ec.value() != 0) {
								std::ostringstream os;
								os << "Cannot create hardlink for \"" << base << " as " << file << "\": " << ec.message() << ".";
								throw std::runtime_error(os.str());
							}
							struct stat base_stat;
							if (stat(base_str, &base_stat) == 0) {
								chown(file_str, base_stat.st_uid, base_stat.st_gid);
								chmod(file_str, base_stat.st_mode);
							}
						}
					}

					saved += size;
				}
				else {
					std::cout << "Owner/mode mismatch " << base << "and" << file << std::endl;
				}
			}
		}
	}

	std::cout << "Done!" << std::endl << "Saved " << std::fixed << std::setprecision(2) << (double)saved / (1024 * 1024) << "MiB" << std::endl;
	return 0;
}
