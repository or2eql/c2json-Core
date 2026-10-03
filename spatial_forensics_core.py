
# Konstanten für den 3D-Kollisionsraum (Parkhaus)
GRID_BITS = 5
GRID_MASK = 0x1F
GRID_SIZE = 32
MAX_32 = 0xFFFFFFFF

def mix_bits(val):
    """
    Der Bit-Mixer: Streut die lokal gebundenen Werte deterministisch 
    über den gesamten 3D-Raum (Avalanche-Effekt).
    """
    val &= MAX_32
    val ^= val >> 16
    val = (val * 0x7feb352d) & MAX_32
    val ^= val >> 15
    val = (val * 0x846ca68b) & MAX_32
    val ^= val >> 16
    return val

def generate_spatial_hash(data_string, key_val=42):
    """
    Kombiniert die asymmetrische Verschlüsselung (Allee-Swap) 
    mit der räumlichen Zuweisung (Parkhaus).
    
    Returns:
        hex_chain: Der klassische kryptografische Output.
        path_3d: Eine Liste von (x,y,z) Koordinaten, die den einzigartigen 
                 holografischen Pfad der Datei im Raum darstellen.
        kollisionen: Anzahl der Raum-Überlappungen.
    """
    a = key_val
    b = len(data_string) + 29
    
    path_3d = []
    encoded_hex = []
    kollisionen = 0
    
    # Das leere 3D-Gitter initialisieren
    parkhaus = [[[None for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]

    for step, char in enumerate(data_string):
        char_val = ord(char)
        
        # 1. Die dynamische Maske (Die asymmetrische Gleichung)
        mask = ((a + b) * (b - a)) ^ 0x1111101
        
        # 2. Bit-Eingriff (Kryptografische Mutation)
        cipher_char = (char_val ^ (mask & 0xFF))
        encoded_hex.append(hex(cipher_char))
        
        # 3. Raumberechnung: Zustand in den Mixer werfen
        mixed_val = mix_bits(cipher_char ^ a ^ b)
        
        # 4. 3D-Koordinaten aus dem mutierten Hash extrahieren
        z = mixed_val & GRID_MASK                       # Bits 0-4
        y = (mixed_val >> GRID_BITS) & GRID_MASK        # Bits 5-9
        x = (mixed_val >> (GRID_BITS * 2)) & GRID_MASK  # Bits 10-14
        
        path_3d.append((x, y, z))
        
        # 5. Räumliche Kollisionsprüfung (Der Integritäts-Beweis)
        if parkhaus[x][y][z] is not None:
            kollisionen += 1
        else:
            parkhaus[x][y][z] = step
            
        # 6. Der deterministische Swap (Folgen-Mutation für den nächsten Zyklus)
        a = (a + char_val) >> 1
        b = (b ^ a) + 7
        a, b = b, a 
        
    return "-".join(encoded_hex), path_3d, kollisionen

if __name__ == "__main__":
    print("[+] SPATIAL FORENSICS ENGINE INITIALIZED")
    
    # Simulierter RAM-Dump oder beschlagnahmte Datei
    target_data = "Die Welt ist dumm aber die 42 bleibt. Das ist der unverfälschte Beweis."
    print(f"[*] Scanne Zieldaten ({len(target_data)} Bytes)...")
    
    hex_output, path, collisions = generate_spatial_hash(target_data)
    
    print("\n[+] KRYPTOGRAFISCHER STROM (HEX-CHAIN):")
    print(hex_output)
    
    print(f"\n[+] RÄUMLICHE METRIKEN (PARKHAUS-MAPPING):")
    print(f"    Pfad-Länge (Schritte): {len(path)}")
    print(f"    Räumliche Kollisionen: {collisions}")
    print("\n[+] ERSTE 5 RAUM-KOORDINATEN (Der holografische Pfad):")
    for i in range(min(5, len(path))):
        print(f"    Schritt {i}: (X:{path[i][0]:02d}, Y:{path[i][1]:02d}, Z:{path[i][2]:02d})")
