import ctypes
import time
import random
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

# ==========================================
# 1. C-Interface (ctypes Binding)
# ==========================================

# C-Struktur für die Metadaten mappen
class SpatialResult(ctypes.Structure):
    _fields_ = [
        ("path_length", ctypes.c_size_t),
        ("collisions", ctypes.c_int)
    ]

# Shared Library laden
try:
    engine = ctypes.CDLL('./libmodul.so')
except OSError:
    print("[!] Fehler: libspatial_forensics.so nicht gefunden. Hast du 'make' ausgeführt?")
    exit(1)

# Funktions-Signatur exakt definieren
# void_p für das 3D-Array ist am sichersten für Pointer-Übergaben in Python
engine.generate_spatial_hash.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t, ctypes.c_void_p]
engine.generate_spatial_hash.restype = SpatialResult

def run_forensics_engine(data_bytes):
    """Jagt rohe Bytes durch die C-Engine und gibt Metadaten + 3D-Grid zurück."""
    # Leeres 3D Array (32x32x32) im RAM allokieren
    GridType = ctypes.c_int * 32 * 32 * 32
    grid = GridType()
    
    # Python-Bytes in ein C-kompatibles Array umwandeln
    data_array = (ctypes.c_ubyte * len(data_bytes))(*data_bytes)
    
    # C-Engine feuern und Zeit stoppen
    start_time = time.perf_counter()
    result = engine.generate_spatial_hash(data_array, len(data_bytes), ctypes.byref(grid))
    end_time = time.perf_counter()
    
    # C-Array für die Visualisierung in ein Numpy-Array umwandeln
    grid_np = np.ctypeslib.as_array(grid).reshape((32, 32, 32))
    
    return result, grid_np, (end_time - start_time) * 1000

# ==========================================
# 2. Test-Szenario: Der Avalanche-Beweis
# ==========================================

print("=== Spatial Forensics: Live In-Memory Test ===")

# 1. Wir generieren eine "Datei" mit 50.000 zufälligen Bytes (~50 KB)
file_size = 50000
original_data = bytearray(random.getrandbits(8) for _ in range(file_size))

# 2. Original-Datei verarbeiten
print(f"\n[*] Generiere 3D-Hash für Original-Datei ({file_size} Bytes)...")
res_orig, grid_orig, time_orig = run_forensics_engine(original_data)
print(f"[+] Original verarbeitet in {time_orig:.4f} ms")
print(f"    Pfadlänge:  {res_orig.path_length}")
print(f"    Kollisionen: {res_orig.collisions}")

# 3. Den Deepfake simulieren (Ein einziges Bit manipulieren!)
manipulated_data = bytearray(original_data)
# Wir ändern Byte Nummer 25.000 um einen winzigen Wert
manipulated_data[25000] = manipulated_data[25000] ^ 0x01 

# 4. Manipulierte Datei verarbeiten
print("\n[*] Generiere 3D-Hash für manipulierte Datei (1 Bit verändert)...")
res_fake, grid_fake, time_fake = run_forensics_engine(manipulated_data)
print(f"[+] Manipulation verarbeitet in {time_fake:.4f} ms")
print(f"    Pfadlänge:  {res_fake.path_length}")
print(f"    Kollisionen: {res_fake.collisions}")

# ==========================================
# 3. Mathematische Verifizierung (Diff)
# ==========================================

# Zählen, in wie vielen der 32.768 Räume (32*32*32) sich der Wert unterscheidet
grid_diff = np.sum(grid_orig != grid_fake)
diff_percent = (grid_diff / (32*32*32)) * 100

print("\n=== Forensisches Resultat ===")
if grid_diff == 0:
    print("[!] FEHLER: Keine Abweichung erkannt.")
else:
    print(f"[+] AVALANCHE-EFFEKT BESTÄTIGT!")
    print(f"[+] 1 verändertes Bit führte zu {grid_diff} veränderten Sektoren im 3D-Raum ({diff_percent:.2f}% Struktur-Kollaps).")

# ==========================================
# 4. 3D-Visualisierung für GitHub/Gitea
# ==========================================
print("\n[*] Rendiere 3D-Beweisbild für README.md...")

fig = plt.figure(figsize=(12, 6))

# Subplot 1: Original
ax1 = fig.add_subplot(121, projection='3d')
x1, y1, z1 = np.nonzero(grid_orig)
ax1.scatter(x1, y1, z1, c='blue', s=1, alpha=0.3)
ax1.set_title(f'Original-Signatur\nKollisionen: {res_orig.collisions}')

# Subplot 2: Manipuliert
ax2 = fig.add_subplot(122, projection='3d')
x2, y2, z2 = np.nonzero(grid_fake)
ax2.scatter(x2, y2, z2, c='red', s=1, alpha=0.3)
ax2.set_title(f'Deepfake-Signatur (1 Bit verändert)\nKollisionen: {res_fake.collisions}')

plt.suptitle("Spatial Forensics: Deterministischer Avalanche-Effekt", fontsize=14)
plt.savefig("spatial_proof.png", dpi=300, bbox_inches='tight')
print("[+] Visualisierung gespeichert unter 'spatial_proof.png'")
