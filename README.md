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