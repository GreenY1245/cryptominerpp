// CryptoMiner.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <picosha2.h>
#include <ctime>
#include <json.hpp>

std::string generateRandomString(const int length) {

	static const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	std::string word;
	
	for (int i = 0; i < length; ++i) {
		word += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return word;
}

int main() {
	
	int nonce = 0, count = 0;
	std::string random;
	
	while (true) {

		// region Get timestamp
		time_t now = time(nullptr);
		struct tm timeinfo;
		gmtime_s(&timeinfo, &now);
		char timestamp[sizeof "YYYY-MM-DDTHH:MM:SSZ"];
		strftime(timestamp, sizeof timestamp, "%FT%TZ", &timeinfo);
		// endregion
		
		std::vector<unsigned char> hash(picosha2::k_digest_size);
		random = generateRandomString(16) + std::to_string(nonce) + timestamp;

		// region Hash block
		picosha2::hash256(random.begin(), random.end(), hash.begin(), hash.end());
		// endregion


		// Check if block hash matches required parameters
		if (picosha2::bytes_to_hex_string(hash.begin(), hash.end()).substr(0, 2) == "00") {

			// JSON construction
			nlohmann::json engine;
			engine["timestamp"] = timestamp;
			engine["data"] = random;
			engine["hash"] = picosha2::bytes_to_hex_string(hash.begin(), hash.end());

			// JSON output
			std::cout << engine.dump() << std::endl;

			// Temporary because there are no HTTP api calls at the moment but the hashing works
			if (count > 5) {
				break;
			}
			count++;
		}

		nonce++;
	}
}
