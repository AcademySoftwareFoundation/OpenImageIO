# Lessons learned running the audit on OIIO (update this list)

Project-specific tips. Lessons that apply equally to OSL belong in
`SKILL.md` instead (see its "Project-specific files" section).

- Confirming "bleeding edge" builds main/master (method in SKILL.md step 2
  notes): a reported version equal to the last release is normal (the
  2026-10-03 check: LibRaw master still says 0.22.0 because 0.22.x patch
  releases come from a branch; pugixml, libtiff, Freetype, OpenJPEG,
  pybind11 master headers also match their latest tag). Never pinned to
  main there: libjxl, robin-map, OpenJPH (tags/defaults).
- Naive `sort -V | tail` on the tag lists of libjpeg-turbo, freetype,
  openjpeg, ffmpeg, and nanobind returns non-release tags; use the filters
  in `project.md`.
