// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/dassert.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
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

template<class T, typename Arena, typename ManagedArena = NullArena>
class Stream {
public:
    template<class, typename, typename> friend class Stream;

    static constexpr bool IsManager
        = !std::is_same<ManagedArena, NullArena>::value;
    using Managed = Stream<T, ManagedArena>;

    // Managed side functionality

    OPT_FUNCT(!IsManager, T&)
    operator[](uint32_t i)
    {
        OIIO_CONTRACT_ASSERT(i < m_size);
        const uint32_t page_index = i / kPageSize;
        const uint32_t off        = i % kPageSize;
        OIIO_CONTRACT_ASSERT(page_index < num_pages());
        tagged_ptr<Page> page = m_pages[page_index];
        return (*page)[off];
    }

    OPT_FUNCT(!IsManager, const T&)
    operator[](uint32_t i) const
    {
        OIIO_CONTRACT_ASSERT(i < m_size);
        const uint32_t page_index = i / kPageSize;
        const uint32_t off        = i % kPageSize;
        OIIO_CONTRACT_ASSERT(page_index < num_pages());
        tagged_ptr<Page> page = m_pages[page_index];
        return (*page)[off];
    }

    OPT_CONSTRUCT(!IsManager)
    Stream()
        : m_pages(nullptr)
        , m_page_capacity(0)
        , m_size(0)
        , m_owner(false)
        , m_arena(nullptr)
    {
    }

    OPT_CONSTRUCT(!IsManager)
    Stream(Arena& arena)
        : m_pages(nullptr)
        , m_page_capacity(0)
        , m_size(0)
        , m_owner(false)
        , m_arena(&arena)
    {
    }

    OPT_CONSTRUCT(!IsManager)
    Stream(const Stream& o)
        : m_pages(o.m_pages)
        , m_page_capacity(o.m_page_capacity)
        , m_size(o.m_size)
        , m_owner(false)
        , m_arena(o.m_arena)
    {
    }

    OPT_FUNCT(!IsManager, const Stream&)
    operator=(const Stream & o)
    {
        m_pages         = o.m_pages;
        m_page_capacity = o.m_page_capacity;
        m_size          = o.m_size;
        m_owner         = false;
        m_arena         = o.m_arena;
        return *this;
    }

    // Manager side functionality

    OPT_CONSTRUCT(IsManager)
    Stream(Arena& arena, ManagedArena& marena, Managed& managed)
        : m_pages(nullptr)
        , m_page_capacity(0)
        , m_size(0)
        , m_owner(true)  // Manager always owns
        , m_arena(&arena)
        , m_managed(managed)
        , m_staging_page(nullptr)
    {
        // Initialize managed-side storage explicitly; assignment would force
        // m_owner=false and break teardown invariants when no pages are grown.
        m_managed.m_pages         = nullptr;
        m_managed.m_page_capacity = 0;
        m_managed.m_size          = 0;
        m_managed.m_owner         = false;
        m_managed.m_arena         = &marena;
    }

    ~Stream()
    {
        if (m_owner) {
            if constexpr (IsManager) {
                for (uint32_t i = 0, end = num_pages(); i < end; ++i)
                    m_managed.m_arena->free(m_pages[i]);
                OIIO_CONTRACT_ASSERT(!m_managed.m_owner);
                m_managed.m_arena->free(m_managed.m_pages);
                m_managed.m_pages = nullptr;
                m_arena->free(m_staging_page);
            }
            m_arena->free(m_pages);
        }
    }

    uint32_t staging_index() const { return m_size % kPageSize; }
    uint32_t staging_page() const { return m_size % kPageSize; }

    OPT_FUNCT(IsManager, void)
    clear()
    {
        if (m_pages) {
            uint32_t npages = num_pages();
            for (uint32_t i = 0; i < npages; ++i) {
                m_managed.m_arena->free(m_pages[i]);
                m_pages[i] = nullptr;
            }
            if (m_managed.m_pages && m_page_capacity) {
                m_managed.m_arena->copy_to(m_managed.m_pages, m_pages,
                                           sizeof(tagged_ptr<Page>)
                                               * m_page_capacity);
            }
        }
        m_size = m_managed.m_size = 0;
    }

    OPT_FUNCT(IsManager, uint32_t)
    push_back(const T& value)
    {
        ensure_space();
        (*m_staging_page)[staging_index()] = value;
        m_size++;
        return m_size - 1;
    }

    OPT_FUNCT(IsManager, void)
    sync_to_managed()
    {
        // Only the mutable tail page needs to be flushed; sealed pages are
        // already synchronized when they are completed.
        if (m_size != m_managed.m_size)
            sync_stage();
    }
    OPT_FUNCT(IsManager, void)
    sync_from_managed() {}

private:
    static constexpr uint32_t kPageSize = uint32_t((64ull * 1024ull * 1024ull)
                                                   / sizeof(T));  // 64MB pages
    static_assert(kPageSize > 0, "Stream page size must be non-zero");

    using Page = std::array<T, kPageSize>;

    uint32_t num_pages() const
    {
        return m_size / kPageSize + (m_size % kPageSize ? 1 : 0);
    }

    OPT_FUNCT(IsManager, void)
    ensure_space()
    {
        if (!m_staging_page) {
            // First time
            grow_page_capacity();
            m_pages[0] = m_managed.m_arena->alloc(sizeof(Page),
                                                  "Stream::push_back");
            m_managed.m_arena->copy_to(m_managed.m_pages, m_pages,
                                       sizeof(tagged_ptr<Page>)
                                           * m_page_capacity);
            m_staging_page = m_arena->alloc(m_pages[0], sizeof(Page),
                                            "Stream::Stream()");
        } else if (m_size && (m_size % kPageSize == 0)) {
            // Make room for the next push_back
            const bool unified_stage = m_pages[staging_page()]
                                       == m_staging_page;
            sync_stage();
            uint32_t req_page_capacity = (m_size + kPageSize) / kPageSize;
            if (req_page_capacity > m_page_capacity)
                grow_page_capacity();
            uint32_t page = req_page_capacity - 1;
            if (!m_pages[page]) {
                // Pages are allocated in managed arena memory so device-side reads
                // can index them directly after sync.
                m_pages[page] = m_managed.m_arena->alloc(sizeof(Page),
                                                         "Stream::push_back");
                m_managed.m_arena->copy_to(m_managed.m_pages, m_pages,
                                           sizeof(tagged_ptr<Page>)
                                               * m_page_capacity);
            }
            if (unified_stage)
                m_staging_page = m_pages[page];
        }
    }

    size_t grow_page_capacity()
    {
        // Both manager and managed
        uint32_t newcap = m_page_capacity ? m_page_capacity * 2 : 2;
        tagged_ptr<tagged_ptr<Page>> old_pages         = m_pages;
        tagged_ptr<tagged_ptr<Page>> old_managed_pages = nullptr;
        if constexpr (IsManager) {
            old_managed_pages = m_managed.m_pages;
            m_managed.m_pages
                = m_managed.m_arena->alloc(sizeof(tagged_ptr<Page>) * newcap,
                                           "Stream::grow_page_capacity");
            m_pages                   = m_arena->alloc(m_managed.m_pages,
                                                       sizeof(tagged_ptr<Page>) * newcap,
                                                       "Stream::grow_page_capacity");
            m_managed.m_page_capacity = newcap;
            m_managed.m_owner         = false;
        } else
            m_pages = m_arena->alloc(nullptr, sizeof(tagged_ptr<Page>) * newcap,
                                     "Stream::grow_page_capacity");
        // Ensure newly allocated pointer slots start null.
        std::fill_n(m_pages.get(), newcap, tagged_ptr<Page>(nullptr));
        m_arena->copy_in(m_pages, old_pages,
                         sizeof(tagged_ptr<Page>) * m_page_capacity);
        m_page_capacity = newcap;
        if constexpr (IsManager) {
            m_managed.m_arena->copy_to(m_managed.m_pages, m_pages,
                                       sizeof(tagged_ptr<Page>)
                                           * m_page_capacity);
            m_arena->free(old_managed_pages);
        }
        if (m_owner)
            m_arena->free(old_pages);
        m_owner = true;
        return m_page_capacity;
    }

    void sync_stage()
    {
        if (m_size == 0)
            return;
        const uint32_t last_page = (m_size - 1) / kPageSize;
        OIIO_CONTRACT_ASSERT(last_page < num_pages());
        // The staging page mirrors the current tail page and is copied as a
        // whole page for simplicity.
        m_managed.m_arena->copy_to(m_pages[last_page], m_staging_page,
                                   sizeof(Page));
        m_managed.m_size = m_size;
    }

    // Both managed and manager
    tagged_ptr<tagged_ptr<Page>> m_pages;
    uint32_t m_page_capacity;
    uint32_t m_size;
    bool m_owner;
    Arena* m_arena;

    // Manager only
    OPT_FIELD(IsManager, Managed&) m_managed;
    OPT_FIELD(IsManager, tagged_ptr<Page>) m_staging_page;
};

}  // namespace texture_device

#undef OPT_FIELD
#undef OPT_CONSTRUCT
#undef OPT_FUNCT
