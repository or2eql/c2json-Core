# c2json-Core: Dynamic JSON & Node Framework

**⚠️ Architektur-Hinweis:** Dieses Repository enthält *ausschließlich* die Core-Engine des c2json-Frameworks. Es handelt sich hierbei um das abstrakte C-Fundament für Speichermanagement, dynamisches Objekt-Mapping und die grundlegende Modul-Lade-Logik. Spezifische Anwendungslogik, Netzwerkimplementierungen oder spezialisierte Swarm-Nodes sind strikt von diesem Core getrennt und werden als externe Module zur Laufzeit nachgeladen.

## 1. Abstract
`c2json-Core` ist ein leichtgewichtiges, in nativem C geschriebenes Trägersystem. Es dient als hochperformantes Bindeglied zwischen dynamischem JSON-Parsing und der Ausführung von asynchronen Knotenpunkten (Nodes). Anstatt alle Funktionen hart in den Code zu kompilieren, operiert der Core als minimalistischer Host, der Payload-Daten verarbeitet und an externe Plugins delegiert. Der Fokus der Architektur liegt auf extremer Speichereffizienz, Pointer-Sicherheit und modularer Skalierbarkeit.

## 2. Kern-Features & Technische Architektur

* **Dynamisches Plugin-Loading:** Das Herzstück des Cores bildet die Fähigkeit, externe Module (Shared Objects / `.so`) zur Laufzeit dynamisch in den Arbeitsspeicher zu laden. Über `dlopen` und `dlsym` werden Funktions-Symbole dynamisch gebunden, was das System hochgradig polymorph und erweiterbar macht, ohne den Core jemals neu kompilieren zu müssen.
* **Low-Level JSON-Parsing:** Integrierte, speicheroptimierte Übersetzung zwischen rohen JSON-Strings und verschachtelten C-Datenstrukturen (teilweise inspiriert von Dave Gambles `cJSON`-Architektur). Der Parser verzichtet auf unnötigen Overhead und mappt JSON-Trees direkt auf saubere RAM-Blöcke.
* **Swarm-Node-Foundation:** Der Core ist architektonisch als Fundament für asynchrone HTTP-Server und verteilte Knotenpunkte (Swarm Nodes) ausgelegt. Er verwaltet den Lebenszyklus der Daten, während die nachgeladenen Module die eigentliche Ausführung übernehmen.
* **Zero-Bloat Philosophie:** Verzicht auf überladene externe Abhängigkeiten. Das Framework nutzt ausschließlich flache C-Strukturen und effiziente Pointer-Arithmetik für maximale Geschwindigkeit.

## 3. Integration & Nutzung
Der Core ist nicht als Standalone-Programm konzipiert, sondern agiert als Blackbox-Engine. Er wird als Bibliothek kompiliert, liest eingehende JSON-Payloads ein, routet die Daten basierend auf ihrer Struktur an die dynamisch geladenen `.so`-Module und gibt das Resultat nach der Verarbeitung sicher zurück.

> **⚠️ Architecture Note:** This repository contains *exclusively* the core engine of the `c2json` framework. It serves as the abstract C foundation for memory management, dynamic object mapping, and basic module loading logic. Specific application logic, network implementations, or specialized swarm nodes are strictly separated from this core and are dynamically loaded as external modules at runtime.

## 1. Abstract
`c2json-Core` is a lightweight host system written in native C. It acts as a high-performance bridge between dynamic JSON parsing and the execution of asynchronous nodes. Instead of hardcoding all functions into the executable, the core operates as a minimalist host that processes payload data and delegates it to external plugins. The architecture strictly focuses on extreme memory efficiency, pointer safety, and modular scalability.

## 2. Core Features & Technical Architecture

* **Dynamic Plugin Loading:** The heart of the core is its ability to dynamically load external modules (Shared Objects / `.so`) into memory at runtime. Using `dlopen` and `dlsym`, function symbols are dynamically bound, making the system highly polymorphic and extensible without ever needing to recompile the core.
* **Low-Level JSON Parsing:** Integrated, memory-optimized translation between raw JSON strings and nested C data structures (partially inspired by Dave Gamble's `cJSON` architecture). The parser eliminates unnecessary overhead, mapping JSON trees directly onto clean RAM blocks.
* **Swarm Node Foundation:** Architecturally, the core is designed as the foundation for asynchronous HTTP servers and distributed execution nodes (Swarm Nodes). It manages the data lifecycle, while the dynamically loaded modules handle the actual execution.
* **Zero-Bloat Philosophy:** Complete absence of bloated external dependencies. The framework relies exclusively on flat C structures and efficient pointer arithmetic to achieve maximum speed.

## 3. Integration & Usage
The core is not designed as a standalone application, but rather operates as a black-box engine. Compiled as a shared library, it reads incoming JSON payloads, routes the data to dynamically loaded `.so` modules based on its structure, and securely returns the result after processing.

### Quickstart / Minimal Example
Here is a conceptual example of how the core dynamically loads a module based on a parsed JSON command:

```c
#include <dlfcn.h>
#include <stdio.h>
#include "c2json_core.h"

int main() {
    // 1. Core parses incoming payload
    const char* json_payload = "{\"plugin\": \"libcoffee.so\", \"action\": \"brew\"}";
    c2json_object* parsed_data = c2json_parse(json_payload);
    
    // 2. Extract target module from JSON
    const char* target_lib = c2json_get_string(parsed_data, "plugin");
    
    // 3. Dynamically load the module (Hotplugging)
    void* handle = dlopen(target_lib, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load module: %s\n", dlerror());
        return 1;
    }
    
    // 4. Bind and execute the function pointer
    void (*execute_action)() = dlsym(handle, "execute_action");
    if (execute_action) {
        execute_action(); // Hands over execution to the .so module
    }
    
    dlclose(handle);
    c2json_free(parsed_data);
    return 0;
}
