#pragma once

#include <new>
#include <esp_heap_caps.h>

/**
 * @brief Allocator que utiliza PSRAM si está disponible, y cae automáticamente a SRAM si no.
 * 
 * Este allocator intenta usar memoria PSRAM para almacenar grandes colecciones,
 * pero si no está disponible o si falla la asignación, utiliza la memoria normal de heap (SRAM).
 * El comportamiento es transparente para el código que lo utiliza.
 */
template <typename T>
class DynamicPsramAllocator {
public:
    // Tipos requeridos para el concepto de Allocator de C++
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

    /**
     * @brief Asigna memoria para n elementos de tipo T
     * 
     * Intenta usar PSRAM si está disponible, de lo contrario usa SRAM.
     * 
     * @param n Número de elementos a asignar
     * @return T* Puntero a la memoria asignada
     */
    pointer allocate(size_type n) {
        if (n == 0) return nullptr;
        
        void* ptr = nullptr;
        bool usePSRAM = psramFound() && ESP.getFreePsram() > n * sizeof(T);
        
        if (usePSRAM) {
            // Intenta primero con PSRAM
            ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (ptr) {
                log_v(">>> Allocated %zu bytes in PSRAM at %p", n * sizeof(T), ptr);
                return static_cast<pointer>(ptr);
            }
        }
        
        // Si falla PSRAM o no está disponible, usa SRAM
        ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_8BIT);
        if (!ptr) {
            // Intentar liberar memoria y reintentar
            ESP.getHeapSize(); // Forzar limpieza de fragmentación
            ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_8BIT);
            
            if (!ptr) {
                throw std::bad_alloc();
            }
        }
        
        log_v(">>> Allocated %zu bytes in SRAM at %p", n * sizeof(T), ptr);
        return static_cast<pointer>(ptr);
    }

    /**
     * @brief Libera memoria previamente asignada
     * 
     * @param p Puntero a la memoria a liberar
     * @param n Número de elementos (no usado pero requerido por el estándar)
     */
    void deallocate(pointer p, size_type n) noexcept {
        if (p) {
            log_v("<<< Deallocating %zu bytes at %p", n * sizeof(T), (void*)p);
            heap_caps_free(p);
        }
    }

    // Métodos requeridos para cumplir con el concepto de Allocator
    bool operator==(const DynamicPsramAllocator&) const noexcept { return true; }
    bool operator!=(const DynamicPsramAllocator&) const noexcept { return false; }
};
