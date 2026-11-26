#include "Batch.hpp"
#include <QueueUtility/Iterator.hpp>

Hit::Hit(HitIterator iter) : toa(iter.toa()), tot(iter.tot()), x(iter.x()), y(iter.y()), delay(iter.delay()),
                                    trigger_diff(iter.trigger_diff()), cluster_tot(iter.cluster_tot()),
                                    cluster_size(iter.cluster_size()) {
}

Hit HitBatch::operator[](size_t index) {
    // Return a Hit by value because the data is stored in SoA format.
    return Hit(
        HitIterator(this, index)
    );
}
