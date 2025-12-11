#pragma once

#include <vector>
#include <variant>
#include <climits>

#include "veb.h"
#include "rht.h"

namespace DeLI {
    
    template<typename T, unsigned int low_bits>
    class DeLI {
    public:
        static constexpr int high_bits = sizeof(T) * CHAR_BIT - low_bits;

        DeLI() {
            top_level.resize(1 << high_bits);
        }

        void insert(T key) {
            
        }

        void remove(T key) {
            
        }

    private:
        std::vector<std::variant<RHT<T>, veb_tree>> top_level;
    };
}
