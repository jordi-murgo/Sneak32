#pragma once

#include <new>
#include <esp_heap_caps.h>

template <typename T>
class DynamicPsramAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind { using other = DynamicPsramAllocator<U>; };

    DynamicPsramAllocator() noexcept {}
    template<typename U>
    DynamicPsramAllocator(const DynamicPsramAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        if (n == 0) return nullptr;
        
        void* ptr = nullptr;
        if (psramFound()) {
            // Intenta primero con PSRAM
            ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (ptr) {
                log_d(">>> Allocated %zu bytes in PSRAM at %p", n * sizeof(T), ptr);
                return static_cast<pointer>(ptr);
            }
        }
        
        // Si falla PSRAM o no está disponible, usa SRAM
        ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_8BIT);
        if (!ptr) {
            throw std::bad_alloc();
        }
        
        log_d(">>> Allocated %zu bytes in SRAM at %p", n * sizeof(T), ptr);
        return static_cast<pointer>(ptr);
    }

    void deallocate(pointer p, size_type n) noexcept {
        if (p) {
            log_d("<<< Deallocating %zu bytes at %p", n * sizeof(T), (void*)p);
            heap_caps_free(p);
        }
    }

    // Métodos requeridos para cumplir con el concepto de Allocator
    bool operator==(const DynamicPsramAllocator&) const noexcept { return true; }
    bool operator!=(const DynamicPsramAllocator&) const noexcept { return false; }
};
