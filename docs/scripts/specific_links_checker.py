# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import os
import re
import requests
from urllib.parse import urlparse
import sys
from collections import defaultdict

# Config URLs to find
URL_REGEXES = [
    re.compile(r'https?://(?:[a-zA-Z0-9.-]+\.)?intel\.com(?:/[^\s<>"\'\)\]]*)?'),
    # add more regexes as needed
]
URL_EXCLUDED_KEYWORDS = ['apt', 'repos', 'repositories']


# Set timeout for HTTP requests
TIMEOUT = 10

def extract_links_from_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    all_links = set()
    for regex in URL_REGEXES:
        raw_links = set(regex.findall(content))
        cleaned_links = {
            link.rstrip('.,);:]>\'"') for link in raw_links
            }
        cleaned_links = {
            link for link in cleaned_links
            if not any(word in link for word in URL_EXCLUDED_KEYWORDS)
        }
        all_links.update(cleaned_links)

    return all_links

def find_all_links(directory):
    link_sources = defaultdict(set)  # link -> set of source files
    allowed_extensions = {'.rst', '.html', '.md', '.yaml'}

    for root, _, files in os.walk(directory):
        for filename in files:
            if not any(filename.lower().endswith(ext) for ext in allowed_extensions):
                continue  # Skip files without matching extension

            filepath = os.path.join(root, filename)
            try:
                links = extract_links_from_file(filepath)
                for link in links:
                    link_sources[link].add(filepath)
            except Exception as e:
                print(f"[WARN] Skipped file due to error: {filepath} – {e}")
    return link_sources

def check_link(url):
    try:
        response = requests.head(url, allow_redirects=True, timeout=TIMEOUT)
        if not (200 <= response.status_code < 400):
            response = requests.get(url, allow_redirects=True, timeout=TIMEOUT)
        return 200 <= response.status_code < 400
    except requests.RequestException:
        return False

def main():
    directory = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    link_sources = find_all_links(directory)
    all_links = sorted(link_sources.keys())

    print(f"🔍 Found {len(all_links)} unique links. Checking them...\n")

    broken_links = []

    for link in all_links:
        ok = check_link(link)
        sources = ", ".join(sorted(link_sources[link]))
        print(f"{'✅' if ok else '❌'} {link}  ← from: {sources}")
        if not ok:
            broken_links.append((link, sources))

    if broken_links:
        print(f"\n❌ {len(broken_links)} broken link(s)")
        for link, sources in broken_links:
            print(f"  - {link}  ← from: {sources}")
        sys.exit(1 if len(broken_links) > 1 else 0)
    else:
        print("\n✅ All links are working.")
        sys.exit(0)

if __name__ == '__main__':
    main()
