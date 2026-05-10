// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <variant>

#include "arena.h"

namespace texture_device {

#define OPT_FUNCT(condition, return_type) \
    template<typename __Q = return_type> std::enable_if_t<condition, __Q>

#define OPT_CONSTRUCT(condition) \
    template<bool __C = condition, typename = std::enable_if_t<__C>>

#define OPT_FIELD(condition, field_type) \
    std::conditional_t<condition, field_type, std::monostate>

template<class Key, class Value, class Hash, typename Arena,
         typename ManagedArena = NullArena>
class ClosedHashMap {
    template<typename T> using Atomic = typename Arena::template Atomic<T>;
    static constexpr bool IsManager
        = !std::is_same<ManagedArena, NullArena>::value;
    struct Slot;


public:
    template<class, class, class, typename, typename>
    friend class ClosedHashMap;

    using Managed = ClosedHashMap<Key, Value, Hash, ManagedArena>;

    // Managed only functionality

    OPT_CONSTRUCT(!IsManager)
    ClosedHashMap(Arena& arena, uint32_t capacity)
        : m_arena(&arena)
        , m_capacity(capacity)
        , m_size(0)
        , m_overflowed(false)
        , m_owner(true)
    {
        m_slots = m_arena->alloc(sizeof(Slot) * capacity,
                                 "ClosedHashMap::ClosedHashMap");
        clear();
    }
    OPT_CONSTRUCT(!IsManager)
    ClosedHashMap(const ClosedHashMap& o)
    {
        m_arena      = o.m_arena;
        m_slots      = o.m_slots;
        m_capacity   = o.m_capacity;
        m_size       = o.m_size;
        m_overflowed = o.m_overflowed;
        m_owner      = false;
    }
    OPT_CONSTRUCT(!IsManager)
    ClosedHashMap()
        : m_arena(nullptr)
        , m_slots(nullptr)
        , m_capacity(0)
        , m_size(0)
        , m_overflowed(false)
        , m_owner(false)
    {
    }

    OPT_FUNCT(!IsManager, const ClosedHashMap&)
    operator=(const ClosedHashMap & o)
    {
        m_arena      = o.m_arena;
        m_slots      = o.m_slots;
        m_capacity   = o.m_capacity;
        m_size       = o.m_size.load();
        m_overflowed = o.m_overflowed.load();
        m_owner      = false;
        return *this;
    }

    // Manager only functionality

    OPT_CONSTRUCT(IsManager)
    ClosedHashMap(Arena& arena, ManagedArena& marena, size_t capacity,
                  Managed& managed)
        : m_arena(&arena)
        , m_capacity(capacity)
        , m_size(0)
        , m_overflowed(false)
        , m_owner(true)
        , m_managed(managed)
    {
        // Do not assign from a temporary Managed(marena, capacity) here.
        // Managed-side operator= intentionally sets m_owner=false (view
        // semantics), and a temporary would free the allocated slots on
        // destruction, leaving dangling pointers in m_managed.
        m_managed.m_arena      = &marena;
        m_managed.m_capacity   = m_capacity;
        m_managed.m_size       = m_size.load();
        m_managed.m_overflowed = m_overflowed.load();
        m_managed.m_owner      = true;
        m_managed.m_slots
            = m_managed.m_arena->alloc(sizeof(Slot) * capacity,
                                       "ClosedHashMap::ClosedHashMap");
        m_slots = m_arena->alloc(sizeof(Slot) * capacity,
                                 "ClosedHashMap::ClosedHashMap");

        // Initialize manager-side slot states. Managed side is synchronized
        // from manager before use.
        clear();
    }

    OPT_FUNCT(IsManager, bool)
    grow() { return resize(std::max(8u, m_capacity * 2u)); }

    OPT_FUNCT(IsManager, void)
    sync_to_managed()
    {
        m_managed.m_arena->copy_to(m_managed.m_slots, m_slots,
                                   sizeof(Slot) * m_capacity);
        m_managed.m_size       = m_size.load();
        m_managed.m_overflowed = m_overflowed.load();
    }
    OPT_FUNCT(IsManager, void)
    sync_from_managed()
    {
        m_managed.m_arena->copy_from(m_slots, m_managed.m_slots,
                                     sizeof(Slot) * m_capacity);
        m_size       = m_managed.m_size.load();
        m_overflowed = m_managed.m_overflowed.load();
    }

    // Both manager and managed

    ~ClosedHashMap()
    {
        if (m_owner)
            m_arena->free(m_slots);
    }

    bool insert(const Key& key, const Value& value)
    {
        if (!m_slots || m_capacity == 0)
            return false;

        if constexpr (IsManager) {
            const uint32_t used = m_size.load();
            if ((uint64_t(used + 1u) * 8u) >= (uint64_t(m_capacity) * 7u)) {
                if (!grow()) {
                    m_overflowed.store(true);
                    return false;
                }
            }
        }

        const uint64_t key_state = slot_state(key);
        const uint32_t idx       = probe_start(key);
        for (uint32_t i = 0; i < m_capacity; ++i) {
            Slot& slot = m_slots[(idx + i) % m_capacity];

            // A reserved slot is being written by another thread; spin until
            // that writer publishes the final state.
            uint64_t observed = slot.state.load();
            while (observed == kReservedState)
                observed = slot.state.load();

            if (observed == key_state && slot.key == key) {
                slot.value = value;
                return true;
            }

            if (observed == kEmptyState) {
                uint64_t expected = kEmptyState;
                // Reserve this slot first so key/value writes are not raced by
                // another inserter probing the same position.
                if (!slot.state.cas(expected, kReservedState))
                    continue;

                slot.key   = key;
                slot.value = value;
                slot.state.store(key_state);
                m_size.fetch_add(1u);
                return true;
            }
        }

        m_overflowed.store(true);
        return false;
    }

    bool find(const Key& key, Value& value) const
    {
        if (!m_slots || m_capacity == 0)
            return false;

        const uint64_t key_state = slot_state(key);
        const uint32_t idx       = probe_start(key);
        for (uint32_t i = 0; i < m_capacity; ++i) {
            const Slot& slot = m_slots[(idx + i) % m_capacity];

            uint64_t observed = slot.state.load();
            while (observed == kReservedState)
                observed = slot.state.load();

            if (observed == kEmptyState)
                // Open addressing invariant: first empty slot means key is not
                // present in this probe chain.
                return false;

            if (observed == key_state && slot.key == key) {
                value = slot.value;
                return true;
            }
        }
        return false;
    }

    uint32_t capacity() const { return m_capacity; }
    uint32_t size() const { return m_size.load(); }
    bool overflowed() const { return m_overflowed.load(); }
    bool failed() const { return size() != 0 || overflowed(); }

    void clear()
    {
        if (!m_slots)
            return;
        for (uint32_t i = 0; i < m_capacity; ++i)
            m_slots[i].state.store(kEmptyState);
        m_size.store(0u);
        m_overflowed.store(false);
    }

    class Iterator {
    public:
        Iterator(const Slot* slot, const Slot* end)
            : m_slot(slot)
            , m_end(end)
        {
            advance_to_occupied();
        }

        const Key& operator*() const { return m_slot->key; }

        Iterator& operator++()
        {
            ++m_slot;
            advance_to_occupied();
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return m_slot != other.m_slot;
        }

    private:
        void advance_to_occupied()
        {
            while (m_slot < m_end) {
                const uint64_t state = m_slot->state.load();
                if (state != kEmptyState && state != kReservedState)
                    break;
                ++m_slot;
            }
        }

        const Slot* m_slot;
        const Slot* m_end;
    };

    Iterator begin() const
    {
        if (!m_slots)
            return Iterator(nullptr, nullptr);
        return Iterator(m_slots.get(), m_slots.get() + m_capacity);
    }

    Iterator end() const
    {
        if (!m_slots)
            return Iterator(nullptr, nullptr);
        return Iterator(m_slots.get() + m_capacity, m_slots.get() + m_capacity);
    }

private:
    struct Slot {
        Key key {};
        Value value {};
        Atomic<uint64_t> state { kEmptyState };
    };


    static constexpr uint64_t kEmptyState    = 0;
    static constexpr uint64_t kReservedState = 1;

    static uint64_t slot_state(const Key& key)
    {
        uint64_t s = static_cast<uint64_t>(Hash {}(key));
        // Reserve 0/1 for sentinel states so all occupied slots use >=2.
        if (s == kEmptyState || s == kReservedState)
            s += 2;
        return s;
    }

    uint32_t probe_start(const Key& key) const
    {
        return static_cast<uint32_t>(slot_state(key) % m_capacity);
    }

    bool resize(uint32_t new_capacity)
    {
        tagged_ptr<Slot> old_slots = m_slots;
        m_slots                    = m_arena->alloc(sizeof(Slot) * new_capacity,
                                                    "ClosedHashMap::resize");
        uint32_t old_capacity      = m_capacity;
        m_capacity                 = new_capacity;
        if constexpr (IsManager) {
            clear();
            // Reinsert so probe positions are rebuilt for the new capacity.
            for (uint32_t i = 0; i < old_capacity; ++i) {
                const Slot& slot     = old_slots[i];
                const uint64_t state = slot.state.load();
                if (state == kEmptyState || state == kReservedState)
                    continue;
                insert(slot.key, slot.value);
            }
            m_managed.resize(new_capacity);
            m_managed.m_arena->copy_to(m_managed.m_slots, m_slots,
                                       sizeof(Slot) * m_capacity);
        }  // Otherwise reallocating slots is enough, manager will copy data

        m_arena->free(old_slots);
        return true;
    }

    Arena* m_arena;
    tagged_ptr<Slot> m_slots;
    uint32_t m_capacity = 0;
    Atomic<uint32_t> m_size { 0 };
    Atomic<bool> m_overflowed { false };
    bool m_owner;
    // Manager only
    OPT_FIELD(IsManager, Managed&) m_managed;
};

}  // namespace texture_device

#undef OPT_FIELD
#undef OPT_CONSTRUCT
#undef OPT_FUNCT
