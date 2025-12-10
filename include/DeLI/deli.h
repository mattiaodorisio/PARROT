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
            size_t index = key >> low_bits;
            if (top_level[index].get_type() == veb_type) {
                if (top_level[index].veb.tree == nullptr) {
                    top_level[index].veb.tree = new veb_tree();
                }
                top_level[index].veb.tree->insert(key & ((1 << low_bits) - 1));
            } else {
                if (top_level[index].rht.table == nullptr) {
                    top_level[index].rht.table = rht<T>::allocate_rht(1 << low_bits);
                }
                top_level[index].rht.table->insert(key & ((1 << low_bits) - 1));
            }
        }

    private:
    
        struct VEB_meta {
            veb_tree * tree;
        };
        
        struct RHT_meta {
            rht<T> * table;
        };

        enum node_type { rht_type, veb_type };

        union meta {
            VEB_meta veb;
            RHT_meta rht;

            // We use the pointer lowest bit to distinguish the type
            node_type get_type() const {
                static_assert(sizeof(veb.tree) == sizeof(uint64_t), "Pointer size mismatch");
                return static_cast<node_type>(*(reinterpret_cast<const uint64_t *>(&veb.tree)) & 0x01ul);
            }    
        };

        std::vector<meta> top_level;
    };

}
