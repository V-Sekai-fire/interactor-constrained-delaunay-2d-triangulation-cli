# tri2d-cli

A small, validated C++ CLI that wraps Geogram's real `GEO::CDT2d` —
constrained Delaunay triangulation of a 2D polygon, with automatic hole
removal — behind a JSON-over-stdin/stdout contract. It replaces an
ad-hoc raw-Delaunay-plus-domain-filter fallback with the actual
equivalent of CGAL's `Mesh_2_Constrained_Delaunay_triangulation_2`:
`GEO::CDT2d` supports boundary-edge constraints directly
(`insert_constraint`) and automatic inside/outside/hole removal
(`remove_external_triangles(true)`), matching CGAL's `mark_domains()`.

This triangulation sequence is shared by two independent consumers:

- **GarmentCodeData v2's `orig_lens` regeneration pipeline** (Python),
  which shells out to the built `tri2d_cli` binary in place of CGAL.
- **`V-Sekai-fire/interactor-sketch-mesh`'s `curvenet.elf` godot-sandbox
  guest**, as `cn::cdt2d_triangulate`, which runs the identical
  point-flattening / constraint-insertion / hole-removal sequence over
  native `float` vectors instead of JSON.

One validated algorithm, two consumers in different languages and
runtimes — so it belongs in neither consumer's own repo, and lives
here instead. See the RFD (linked below) for the full reasoning.

## CLI contract

Input JSON on stdin:

```json
{
  "boundary": [[x, y], ...],       // polygon boundary, closed implicitly
  "holes": [[[x, y], ...], ...],   // optional hole polygons
  "interior_points": [[x, y], ...] // optional extra interior points
}
```

Output JSON on stdout:

```json
{
  "points": [[x, y], ...],
  "faces": [[i, j, k], ...]        // indices into "points"
}
```

Points are flattened in order (boundary, then each hole, then interior
points) before triangulation; boundary and hole rings are each inserted
as a closed constraint loop, then `remove_external_triangles(true)`
strips everything outside the boundary and inside any hole. Output
points/faces are remapped to only the vertices that survive, in
ascending internal-vertex-id order.

## Control test

A 10x10 square boundary with a 2x2 hole and 2 interior points
triangulates to exactly area **96.0** (100 - 4), with no triangle
centroid inside the hole:

```json
{"boundary": [[0,0],[10,0],[10,10],[0,10]],
 "holes": [[[4,4],[6,4],[6,6],[4,6]]],
 "interior_points": [[2,2],[8,8]]}
```

```
echo '<input above>' | ./build/tri2d_cli
```

Sum the triangle areas from the output `points`/`faces` and confirm the
total is `96.0`.

## Build

Standalone, no external dependencies beyond a C++17 toolchain and CMake
(the vendored `vendor/geogram-subset/` subtree below supplies Geogram):

```
cmake -B build .
cmake --build build
echo '{"boundary":[[0,0],[10,0],[10,10],[0,10]],"holes":[[[4,4],[6,4],[6,6],[4,6]]],"interior_points":[[2,2],[8,8]]}' | ./build/tri2d_cli
```

## Layout

- `tri2d_cli.cpp` — the CLI: tiny hand-rolled JSON parser for this
  fixed schema, flattening/constraint logic, `GEO::CDT2d` calls, JSON
  output.
- `boolean_expression.cpp` — minimal `GEO::BooleanExpression` (used by
  `ExactCDT2d::classify_triangles`); `geogram/delaunay/CDT_2d.cpp`
  references these symbols at link time even though this CLI never
  calls `ExactCDT2d::classify_triangles`, because `CDT2d` and
  `ExactCDT2d` live in the same translation unit. Ported from upstream
  Geogram (`geogram/basic/boolean_expression.cpp`), same BSD-3-Clause
  license as the rest of the vendored subset.
- `vendor/geogram-subset/` — vendored subset of Geogram 1.9.9
  (`basic`, `numerics`, `mesh`, `delaunay`, `points`, `api`,
  `bibliography`, and the third-party `OpenNL`/predicate-generator/
  numerics pieces it depends on, plus `zlib.h`/`zconf.h` headers only).
  See its own `LICENSE` and `CITATION.cff`.

## RFD

See `V-Sekai-fire/v-sekai-fabric`'s RFD register for the "why its own
repo" writeup (one validated algorithm, two consumers in different
languages/runtimes).

## Licence

MIT for this CLI and `boolean_expression.cpp` (our code) — see
[LICENSE](LICENSE).

The vendored `vendor/geogram-subset/` subtree is **not** covered by
that MIT grant: it is Geogram 1.9.9 (Bruno Lévy / Inria), kept under
its own BSD-3-Clause license and notice at
`vendor/geogram-subset/LICENSE` and `vendor/geogram-subset/CITATION.cff`.
