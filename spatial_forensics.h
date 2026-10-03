#ifndef SPATIAL_FORENSICS_H
#define SPATIAL_FORENSICS_H

#include <stddef.h>
#include <stdint.h>

// Eine simple Struktur für die Rückgabe der Metadaten
typedef struct {
    size_t path_length;
    int collisions;
} SpatialResult;

// Die einzige öffentliche API-Funktion
// Nimmt die rohen Daten, die Länge und einen Zeiger auf das leere 3D-Array.
SpatialResult generate_spatial_hash(const unsigned char *data, size_t length, int grid[32][32][32]);

#endif // SPATIAL_FORENSICS_H
