#include "spatial_forensics.h"

#define GRID_MASK 0x1F
#define GRID_BITS 5

// static = privat. Nur diese Datei kennt den Bit-Mixer.
static uint32_t mix_bits(uint32_t val) {
    val ^= val >> 16;
    val *= 0x7feb352d;
    val ^= val >> 15;
    val *= 0x846ca68b;
    val ^= val >> 16;
    return val;
}

// Die Implementierung deiner API
SpatialResult generate_spatial_hash(const unsigned char *data, size_t length, int grid[32][32][32]) {
    SpatialResult result = {0, 0}; // Initialisiere Metadaten mit 0
    
    // Die asymmetrischen Anker
    uint32_t a = 42;
    uint32_t b = length + 29;

    for (size_t step = 0; step < length; step++) {
        uint32_t char_val = (uint32_t)data[step];
        
        // Dynamische Maske
        uint32_t mask = ((a + b) * (b - a)) ^ 0x1111101;
        uint32_t cipher_char = (char_val ^ (mask & 0xFF));
        
        // In den Mixer werfen
        uint32_t mixed_val = mix_bits(cipher_char ^ a ^ b);
        
        // Koordinaten extrahieren
        uint32_t z = mixed_val & GRID_MASK;
        uint32_t y = (mixed_val >> GRID_BITS) & GRID_MASK;
        uint32_t x = (mixed_val >> (GRID_BITS * 2)) & GRID_MASK;
        
        // Kollisionen tracken oder Raster belegen
        if (grid[x][y][z] != 0) {
            result.collisions++;
        } else {
            grid[x][y][z] = step + 1; 
        }
        
        // Deterministischer Swap
        a = (a + char_val) >> 1;
        b = (b ^ a) + 7;
        
        result.path_length++;
    }

    return result;
}
