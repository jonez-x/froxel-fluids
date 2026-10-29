# Recherche: Was gibt es schon, was machen wir?

Stand: 29.10.2026

## Kurz

Frustum-Gitter, Detail über einer groben Simulation und das Wiederverwenden von Froxel-Daten über Frames gibt es alle schon. Offen ist ein schmaler Fall: einen **simulierten Detailzustand** auf dem Froxel-Gitter bei Kamerabewegung **vom alten ins neue Frustum mitzunehmen**, statt ihn jeden Frame neu zu erzeugen. Woo (2006) nennt genau das als offene Future Work.

## Was es schon gibt

| Baustein | Gibt es? | Quelle |
|---|---|---|
| Rauchsimulation direkt auf einem Frustum-Gitter | ja, nur mit fester Kamera | Woo 2006 |
| Frustum-Detailgitter mit Noise über grober Simulation, One-way | ja, pro Frame neu berechnet (patentiert) | Lucasfilm, US 8,725,476 |
| Kamera-Frustum als Simulationsfenster | ja, pro Shot fest | Pixar, US 9,367,956 |
| Auflösung, die dem Betrachter folgt | ja, mit weltachsenparallelen Gittern | Gao et al. 2013 |
| Frustum-Up-Res in der Filmproduktion | ja, ohne Zustand („frame independent“) | ILM, Fast Fluid Up-Res 2025 |
| Curl Noise / Detail aus der groben Strömung | ja | Bridson 2007, Kim 2008 |
| Froxel-Daten über Frames wiederverwenden | ja, aber nur für Nebel-Beleuchtung | Unreal, Godot, Frostbite |
| Öffentlicher Code für Froxel-Fluidsimulation | nicht gefunden | GitHub-Suche |

## Was wir machen

Grobes Weltgitter (Stable Fluids) plus kameraausgerichtetes Froxel-Gitter mit eigenem, persistentem Detail (feine Dichte und Detailzustand). Bewegt sich die Kamera, wird dieser Zustand ins neue Frustum übertragen. Neu sichtbare Froxel starten aus dem Weltgitter. Verglichen wird Transport gegen Reset.

Wichtig: Ist das Detail reines, zustandsloses Noise, gibt es nichts zu transportieren. Dann gibt es auch keinen Beitrag.

## So formulieren wir es

> Unseres Wissens hält keine bekannte Arbeit einen persistenten, simulierten Detailzustand auf einem kameraausgerichteten Froxel-Gitter und überträgt ihn bei Kamerabewegung explizit vom vorherigen ins neue Frustum.

Nicht behaupten: erstes Frustum-Gitter, erstes Detail über grober Simulation, erste Froxel-Reprojektion.

## Noch prüfen, bevor wir „neu“ sagen

- „Temporal Foveated Fluid Animation in Virtual Reality“ (IEEE TVCG 2026) im Volltext lesen. Einziger aktueller Treffer mit zeitlichem Aspekt.
- Google Scholar „Zitiert von“ für Woo 2006 und Gao 2013 durchsehen.
- GitHub-Code-Suche und Shadertoy nach „froxel“ + „advect“.

## Quellen

- Woo 2006, Realistic Smoke Simulation Using a Frustum Aligned Grid: https://www.cs.ubc.ca/~rbridson/docs/awoo_msc.pdf
- US 8,725,476 (Lucasfilm): https://patents.google.com/patent/US8725476B1/en
- US 9,367,956 (Pixar): https://patents.google.com/patent/US9367956B2/en
- Gao et al. 2013, View-Dependent Multiscale Fluid Simulation: https://cg.cs.tsinghua.edu.cn/papers/TR_110830_gy.pdf
- ILM, Fast Fluid Up-Res (SIGGRAPH 2025 Talk): https://history.siggraph.org/?p=227366
- Kim et al. 2008, Wavelet Turbulence: https://www.cs.cornell.edu/~tedkim/WTURB
- Unreal Volumetric Fog: https://docs.unrealengine.com/4.27/BuildingWorlds/FogEffects/VolumetricFog/
