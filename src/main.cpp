#include <chrono>
#include <iostream>
#include <numeric>
#include "Parser.hpp"
#include "Utility/Memory_Map.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

constexpr auto num_runs = 1;

void printHistogram(const std::vector<double>& runtimes, int numBuckets = 10,
                    const std::string& unit = "ms") {
    if (runtimes.empty() || numBuckets <= 0) {
        std::cout << "No runtime data available.\n";
        return;
    }

    auto sorted = runtimes;
    std::sort(sorted.begin(), sorted.end());

    const double min_hist = std::floor(sorted.front());
    const double max_hist = std::ceil(sorted.back());
    const double mean =
        std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();

    const double median =
        sorted.size() % 2
            ? sorted[sorted.size() / 2]
            : (sorted[sorted.size() / 2 - 1] + sorted[sorted.size() / 2]) / 2.0;

    // Calculate histogram buckets
    std::vector<int> buckets(numBuckets, 0);

    double bucketWidth = (max_hist - min_hist) / numBuckets;

    if (bucketWidth == 0.0) bucketWidth = 1.0;

    for (double runtime : runtimes) {
        int index = static_cast<int>((runtime - min_hist) / bucketWidth);

        index = std::clamp(index, 0, numBuckets - 1);
        buckets[index]++;
    }

    const int maxCount = *std::max_element(buckets.begin(), buckets.end());

    constexpr int barWidth = 40;

    std::cout << "\n";
    std::cout << "Runtime distribution\n";
    std::cout << "-----------------------------------------------\n";

    for (int i = 0; i < numBuckets; ++i) {
        double lower = min_hist + i * bucketWidth;
        double upper = lower + bucketWidth;

        int width = static_cast<int>(
            std::round(static_cast<double>(buckets[i]) / maxCount * barWidth));

        std::cout << std::fixed << std::setprecision(2) << std::setw(8) << lower
                  << " - " << std::setw(8) << upper << " " << unit << " | ";

        for (int j = 0; j < width; ++j) std::cout << '#';

        std::cout << " " << buckets[i] << '\n';
    }

    std::cout << "-----------------------------------------------\n";

    const double min = sorted.front();
    const double max = sorted.back();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Samples: " << runtimes.size() << '\n';
    std::cout << "Min:     " << min << " " << unit << '\n';
    std::cout << "Max:     " << max << " " << unit << '\n';
    std::cout << "Mean:    " << mean << " " << unit << '\n';
    std::cout << "Median:  " << median << " " << unit << '\n';
}

int main(int argc, char* argv[]) {
    std::string filename = "tpx3.tpx3";
    if (argc == 2) filename = argv[1];
    MemoryMap mmap(filename);
    const auto file_size_gb =
        std::distance(mmap.begin(), mmap.end()) * sizeof(uint64_t) * 1e-9;
    Parser<4> parser(std::move(mmap));
    std::cout << "Starting parsing" << std::endl;
    std::vector<double> time_per_run;
    for (int i = 0; i < num_runs; ++i) {
        const auto start = std::chrono::steady_clock::now();
        parser.parse();
        const auto stop = std::chrono::steady_clock::now();
        const auto time_s = std::chrono::duration<double>(stop - start).count();
        time_per_run.push_back(file_size_gb / time_s);
        std::cout << "." << std::flush;
    }

    printHistogram(time_per_run, 10, "Gb/s");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
