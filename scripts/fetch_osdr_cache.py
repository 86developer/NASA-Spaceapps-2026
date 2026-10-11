"""Descarga la respuesta real de la API de OSDR y la guarda en data/.

El archivo data/osdr_live_cache.json no contiene numeros escritos a mano.
Cada elemento de "responses" es el cuerpo JSON que devolvio
https://osdr.nasa.gov/osdr/data/search, sin reescribir campos ni metricas.

La consulta pide estudios de Homo sapiens con Project Type Spaceflight en la
fuente autoritativa de NASA (cgene). Asi el cache no arrastra los datasets
de higado de raton que el equipo descarto (OSD-48, OSD-137, GLDS-242).

Uso, desde la raiz del repo:

    python scripts/fetch_osdr_cache.py
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from datetime import datetime, timezone
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

SEARCH_ENDPOINT = "https://osdr.nasa.gov/osdr/data/search"
API_DOCS = "https://genelab.nasa.gov/genelabAPIs"
USER_AGENT = "VitalGrid-OSDR-cache/1.0"
PAGE_SIZE = 100
TIMEOUT_SECONDS = 60
# OSDR separa valores multiples de un campo con dos o mas espacios.
MULTI_VALUE = re.compile(r"\s{2,}")
# Datasets de higado de raton que el equipo ya descarto. Si la consulta los
# trae, el cache no se escribe.
EXCLUDED_ACCESSIONS = frozenset({"OSD-48", "OSD-137", "GLDS-242"})

# Pares ffield/fvalue documentados en la Study Dataset Search API.
FILTERS = (
    ("organism", "Homo sapiens"),
    ("Project Type", "Spaceflight"),
)


def build_search_url(offset: int) -> str:
    params = [
        ("from", offset),
        ("size", PAGE_SIZE),
        ("type", "cgene"),
    ]
    for field, value in FILTERS:
        params.append(("ffield", field))
        params.append(("fvalue", value))
    return SEARCH_ENDPOINT + "?" + urlencode(params)


def fetch_json(url: str) -> dict:
    request = Request(
        url,
        headers={
            "Accept": "application/json",
            "User-Agent": USER_AGENT,
        },
    )
    try:
        with urlopen(request, timeout=TIMEOUT_SECONDS) as response:
            raw = response.read()
    except HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")[:500]
        raise SystemExit(f"OSDR respondio HTTP {exc.code} para {url}\n{detail}") from exc
    except URLError as exc:
        raise SystemExit(f"No se pudo contactar a OSDR: {exc.reason}") from exc

    try:
        body = json.loads(raw.decode("utf-8"))
    except json.JSONDecodeError as exc:
        raise SystemExit(f"OSDR no devolvio JSON en {url}: {exc}") from exc

    if not isinstance(body, dict) or "hits" not in body:
        raise SystemExit(f"La respuesta de {url} no tiene la forma de la Search API.")
    return body


def hit_total(body: dict) -> int:
    total = body["hits"]["total"]
    if isinstance(total, dict):
        return int(total["value"])
    return int(total)


def fetch_all_pages() -> list[dict]:
    pages = []
    offset = 0
    while True:
        url = build_search_url(offset)
        body = fetch_json(url)
        hits = body["hits"].get("hits") or []
        if not isinstance(hits, list):
            raise SystemExit("OSDR devolvio hits.hits con un tipo inesperado.")
        pages.append({"request_url": url, "body": body})

        total = hit_total(body)
        offset += PAGE_SIZE
        if offset >= total or not hits:
            break
    return pages


def field_values(raw: object) -> list[str]:
    if not isinstance(raw, str) or not raw.strip():
        return []
    return [part.strip() for part in MULTI_VALUE.split(raw) if part.strip()]


def assert_human_spaceflight(pages: list[dict]) -> int:
    """Falla si la API no devolvio el catalogo que pedimos. No reescribe el cuerpo."""
    count = 0
    for page in pages:
        for hit in page["body"]["hits"]["hits"]:
            source = hit.get("_source") or {}
            accession = source.get("Accession") or source.get("Study Identifier") or "?"
            organisms = field_values(source.get("organism"))
            project_types = field_values(source.get("Project Type"))
            identifiers = {accession, source.get("Identifiers"), source.get("Study Identifier")}
            if identifiers & EXCLUDED_ACCESSIONS:
                raise SystemExit(
                    f"{accession} es un dataset de higado de raton que el equipo descarto. "
                    "No se escribe el cache."
                )
            if "Homo sapiens" not in organisms:
                raise SystemExit(
                    f"{accession} no incluye Homo sapiens (organismo={source.get('organism')!r}). "
                    "No se escribe el cache."
                )
            if not any("Spaceflight" in value for value in project_types):
                raise SystemExit(
                    f"{accession} no es un estudio de vuelo espacial "
                    f"(Project Type={source.get('Project Type')!r}). No se escribe el cache."
                )
            count += 1
    if count == 0:
        raise SystemExit("OSDR devolvio cero estudios. No se escribe un cache vacio.")
    return count


def write_cache(path: str, pages: list[dict]) -> None:
    document = {
        "source": "NASA Open Science Data Repository",
        "documentation": API_DOCS,
        "endpoint": SEARCH_ENDPOINT,
        "fetched_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "query": {
            "type": "cgene",
            "from": 0,
            "size": PAGE_SIZE,
            "filters": [{"ffield": field, "fvalue": value} for field, value in FILTERS],
        },
        "responses": pages,
    }

    directory = os.path.dirname(path) or "."
    os.makedirs(directory, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".osdr_live_cache.", suffix=".json", dir=directory)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(document, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
        os.replace(temporary, path)
    except Exception:
        if os.path.exists(temporary):
            os.remove(temporary)
        raise


def default_output_path() -> str:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(root, "data", "osdr_live_cache.json")


def main() -> None:
    parser = argparse.ArgumentParser(description="Guarda la respuesta viva de la API de OSDR.")
    parser.add_argument(
        "--output",
        default=default_output_path(),
        help="Ruta del cache JSON (por defecto data/osdr_live_cache.json).",
    )
    args = parser.parse_args()

    pages = fetch_all_pages()
    count = assert_human_spaceflight(pages)
    write_cache(args.output, pages)

    accessions = []
    for page in pages:
        for hit in page["body"]["hits"]["hits"]:
            source = hit.get("_source") or {}
            accessions.append(source.get("Accession") or "?")

    print(f"Cache escrito en {args.output}")
    print(f"Estudios Homo sapiens / Spaceflight: {count}")
    print("Accesiones: " + ", ".join(accessions))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
