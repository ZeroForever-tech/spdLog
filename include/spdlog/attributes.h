#pragma once

#include <spdlog/common.h>

#include <map>
#include <mutex>


namespace spdlog {

class SPDLOG_API log_attributes {
public:
    using attr_map_t = std::map<std::string, std::string>;
    using key_t = attr_map_t::key_type;
    using value_t = attr_map_t::mapped_type;
    using const_iter = attr_map_t::const_iterator;

    log_attributes() = default;
    log_attributes(const log_attributes& other) { put(other.get_map()); }
    log_attributes& operator=(const log_attributes& other) {
        if (this != &other) {
            clear();
            put(other.get_map());
        }
        return *this;
    }

    void put(attr_map_t const& attributes) {
        auto lck = lock();
        attrs.insert(attributes.begin(), attributes.end());
    }
    void put(const key_t& key, const value_t& value) { put({{key, value}}); }

    void remove(const key_t& key) {
        auto lck = lock();
        auto value_it = attrs.find(key);
        if (value_it != attrs.end()) {
            attrs.erase(key);
        }
    }

    void clear() {
        auto lck = lock();
        attrs.clear();
    }

    bool empty() const {
        auto lck = lock();
        return attrs.empty();
    }

    // return {true, iter} if found, {false, end} otherwise
    std::pair<bool, const_iter> const get(key_t const& key) const {
        auto lck = lock();
        auto value_it = attrs.find(key);

        return std::make_pair(value_it != attrs.end(), value_it);
    }

    // provide a local copy of the attributes to be free of race issues
    // alternative is to lock the attr_map while the formatter iterates over it
    attr_map_t get_map() const {
        auto lck = lock();
        return attrs;
    }

    template <typename Attribute_Iter>
    void attr_ctx(Attribute_Iter begin, Attribute_Iter end) {
        auto lck = lock();
        attrs.insert(begin, end);
    }

    void attr_ctx(std::initializer_list<attr_map_t::value_type> attributes) { attr_ctx(attributes.begin(), attributes.end()); }

    bool empty() {
        auto lck = lock();
        return attrs.empty();
    }

private:
    std::lock_guard<std::mutex> lock() const { return std::lock_guard(attrs_mtx); }

    mutable std::mutex attrs_mtx;
    attr_map_t attrs;
};

}  // namespace spdlog