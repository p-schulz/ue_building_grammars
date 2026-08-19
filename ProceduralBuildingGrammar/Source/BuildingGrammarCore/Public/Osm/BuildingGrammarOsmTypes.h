#pragma once

// Unambiguous public entry point for BuildingGrammarCore's file-parser OSM model. FlexNetwork also
// exposes a public "Osm/OsmTypes.h", but it is a different reflected data-asset model. Including
// this uniquely named header ensures dependency include-path order can never substitute one model
// for the other. The relative include resolves beside this file by construction.
#include "OsmTypes.h"
