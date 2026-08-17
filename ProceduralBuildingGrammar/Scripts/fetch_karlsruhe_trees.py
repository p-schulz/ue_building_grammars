#!/usr/bin/env python3
"""Fetch the complete Karlsruhe Baumkataster (tree cadastre) dataset from the city's ArcGIS
FeatureServer, paging past its per-request record cap, and merge every page into one GeoJSON
FeatureCollection.

Why this exists: a single query against the endpoint below stops at the server's own
resultRecordCount cap (observed as 2000 features) and reports the truncation via a
top-level `"properties": {"exceededTransferLimit": true}` field -- not a page you'll notice reading
the file, since ArcGIS returns it minified as one giant single line with no whitespace. This script
re-issues the same query with an increasing `resultOffset` (0, PAGE_SIZE, 2*PAGE_SIZE, ...) until a
page comes back with fewer features than requested, which is the unambiguous "that was the last
page" signal regardless of the server's own internal cap.

Usage:
    python fetch_karlsruhe_trees.py [-o OUTPUT_PATH] [--page-size N]

No third-party dependencies (stdlib only: urllib, json) -- nothing to pip install.
"""

import argparse
import json
import ssl
import sys
import urllib.error
import urllib.parse
import urllib.request

try:
    # Falls back to Python's own default trust store if certifi isn't installed -- only used to
    # work around environments (common on Windows) where that default store can't verify a normal
    # public CA chain. Not a security downgrade either way: this always verifies against SOME
    # trusted CA bundle, never disables verification.
    import certifi

    _SSL_CONTEXT = ssl.create_default_context(cafile=certifi.where())
except ImportError:
    _SSL_CONTEXT = ssl.create_default_context()

# Same endpoint/query the user already used (where=stadtteil IS NOT NULL, outFields=..., f=geojson)
# -- only resultOffset/resultRecordCount are varied per page.
BASE_URL = (
    "https://geoportal.karlsruhe.de/ags04/rest/services/Hosted/Baumkataster/FeatureServer/2/query"
)
BASE_PARAMS = {
    "where": "stadtteil IS NOT NULL",
    "outFields": "lfdbnr,artdeut,artlat,baumart_allgemein,baumgruppe,stadtteil",
    "returnGeometry": "true",
    "f": "geojson",
}

DEFAULT_PAGE_SIZE = 2000
DEFAULT_OUTPUT = "query_full.geojson"


def fetch_page(offset: int, page_size: int) -> dict:
    params = dict(BASE_PARAMS)
    params["resultOffset"] = str(offset)
    params["resultRecordCount"] = str(page_size)
    url = f"{BASE_URL}?{urllib.parse.urlencode(params)}"

    request = urllib.request.Request(url, headers={"User-Agent": "fetch_karlsruhe_trees.py"})
    with urllib.request.urlopen(request, timeout=60, context=_SSL_CONTEXT) as response:
        return json.load(response)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-o", "--output", default=DEFAULT_OUTPUT, help=f"Output GeoJSON path (default: {DEFAULT_OUTPUT})")
    parser.add_argument("--page-size", type=int, default=DEFAULT_PAGE_SIZE, help=f"Features requested per page (default: {DEFAULT_PAGE_SIZE})")
    args = parser.parse_args()

    all_features = []
    offset = 0
    page_index = 0

    while True:
        print(f"Fetching page {page_index} (offset={offset}, page_size={args.page_size})...", file=sys.stderr)
        try:
            page = fetch_page(offset, args.page_size)
        except (urllib.error.URLError, urllib.error.HTTPError) as exc:
            print(f"Request failed at offset {offset}: {exc}", file=sys.stderr)
            return 1

        if "error" in page:
            print(f"Server returned an error at offset {offset}: {page['error']}", file=sys.stderr)
            return 1

        features = page.get("features", [])
        all_features.extend(features)
        print(f"  got {len(features)} feature(s) (total so far: {len(all_features)})", file=sys.stderr)

        if len(features) < args.page_size:
            # Fewer features than requested -- this was the last page, regardless of what
            # exceededTransferLimit says.
            break

        offset += args.page_size
        page_index += 1

    merged = {
        "type": "FeatureCollection",
        "features": all_features,
        "properties": {"exceededTransferLimit": False},
    }

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(merged, f)

    print(f"Wrote {len(all_features)} total feature(s) to '{args.output}'.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
