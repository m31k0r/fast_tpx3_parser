#include <ctime>
#include <execution>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include "Utility/Memory_Map.hpp"
#include "Parser.hpp"
#include "clock_correction.hpp"
#include "Parsing/tpx3_helper.hpp"

class MaxDisplacement {
   public:
    using Timestamp = long long;

    explicit MaxDisplacement(std::size_t k) : k_(k) {}

    void add(Timestamp timestamp) {
        buffer_.push({timestamp, nextIndex_++});

        // A k-displaced stream can release the smallest
        // timestamp once k+1 elements are buffered.
        while (buffer_.size() > k_) {
            processNext();
        }
    }

    auto get() -> size_t {
        while (!buffer_.empty()) {
            processNext();
        }
        if (maxDisplacement_ == k_)
            std::cerr << "MaxDis limit reached" << std::endl;
        return maxDisplacement_;
    }

   private:
    std::size_t k_;
    std::size_t nextIndex_ = 0;
    std::size_t sortedIndex_ = 0;
    std::size_t maxDisplacement_ = 0;

    // Sort by timestamp, then original index for duplicates.
    using Entry = std::pair<Timestamp, std::size_t>;

    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry> >
        buffer_;

    void processNext() {
        const auto [timestamp, originalIndex] = buffer_.top();
        buffer_.pop();

        const std::size_t displacement = originalIndex > sortedIndex_
                                             ? originalIndex - sortedIndex_
                                             : sortedIndex_ - originalIndex;

        maxDisplacement_ = std::max(maxDisplacement_, displacement);

        ++sortedIndex_;
    }
};

int main(int argc, char *argv[]) {
    std::string filename = "tpx3.tpx3";
    if (argc == 2) filename = argv[1];
    MemoryMap mmap(filename);
    // parse(mmap);
    Parser<2> parser(std::move(mmap));
    std::cout << "Starting parsing" << std::endl;
    parser.parse();
    return 0;
}
