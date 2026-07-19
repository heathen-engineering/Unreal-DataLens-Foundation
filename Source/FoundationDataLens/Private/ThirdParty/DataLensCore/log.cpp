// Thin includer: compiles DataLens Core's own source unmodified. UBT only auto-discovers
// .cpp files under this module's Public/Private tree, so this forwards into the submodule
// (extern/DataLens) rather than duplicating its content -- stays in sync automatically on
// submodule update, nothing to re-copy.
#include "../../../../../extern/DataLens/Core/src/log.cpp"
